#!/usr/bin/env python3
"""Local native-library media validation. No public media services are used."""

from __future__ import annotations

import copy
import json
from contextlib import contextmanager
import sqlite3
import xml.etree.ElementTree as ET
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from core.engine import EngineProcess
from core.report import run_validation
from core.runtime import RunDirectory, process_options
from core.services import CaddyService, WireMockService, post_json
from media.common import command, control_action, decoded_hash, native_env, probe, wait_duration
from media.timeline import validate_epoch_timeline


def wait(
    engine: EngineProcess, gid: str, state: str = "complete", timeout: float = 30
) -> dict:
    return engine.rpc.wait_status(gid, state, timeout)


@contextmanager
def live_source(run, source, ffmpeg, protocol, name, duration=None):
    directory = run.fixtures / name
    directory.mkdir()
    suffix = "m3u8" if protocol == "hls" else "mpd"
    manifest = directory / f"index.{suffix}"
    args = [
        ffmpeg,
        "-v",
        "error",
        "-re",
        "-stream_loop",
        "-1",
        "-i",
        str(source),
        "-map",
        "0",
        "-c",
        "copy",
    ]
    if duration:
        args += ["-t", str(duration)]
    args += (
        ["-f", "hls", "-hls_time", "2", "-hls_list_size", "5"]
        if protocol == "hls"
        else [
            "-f",
            "dash",
            "-seg_duration",
            "2",
            "-window_size",
            "5",
            "-extra_window_size",
            "0",
        ]
    )
    with (run.logs / f"{name}.producer.log").open("wb") as log:
        producer = subprocess.Popen(
            args + [manifest.as_posix()],
            stdin=subprocess.PIPE,
            stdout=log,
            stderr=log,
            env=native_env(),
            **process_options(),
        )
        try:
            deadline = time.monotonic() + 15
            while not manifest.exists():
                if time.monotonic() >= deadline or producer.poll() is not None:
                    raise RuntimeError(f"{protocol} producer did not start")
                time.sleep(0.1)
            yield f"{name}/{manifest.name}"
        finally:
            if producer.poll() is None:
                producer.stdin.write(b"q\n")
                producer.stdin.flush()
                try:
                    producer.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    producer.terminate()
                    producer.wait(timeout=5)
            producer.stdin.close()


def validate(run: RunDirectory, engine_path: Path | None) -> dict:
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError(
            "Fixture generation requires FFmpeg and ffprobe (not engine runtime dependencies)"
        )
    source = run.fixtures / "source.mp4"
    command(
        ffmpeg,
        "-v",
        "error",
        "-f",
        "lavfi",
        "-i",
        "testsrc2=size=160x90:rate=15",
        "-f",
        "lavfi",
        "-i",
        "sine=frequency=440:sample_rate=48000",
        "-t",
        "6",
        "-c:v",
        "libx264",
        "-preset",
        "ultrafast",
        "-g",
        "30",
        "-c:a",
        "aac",
        str(source),
    )
    for name in ("hls", "fmp4", "fmp4-switch", "dash", "byterange", "encrypted"):
        directory = run.fixtures / name
        directory.mkdir()
        args = [ffmpeg, "-v", "error", "-i", str(source), "-map", "0", "-c", "copy"]
        if name == "dash":
            args += [
                "-f",
                "dash",
                "-seg_duration",
                "2",
                (directory / "index.mpd").as_posix(),
            ]
        else:
            args += ["-f", "hls", "-hls_time", "2", "-hls_list_size", "0"]
            if name.startswith("fmp4"):
                args += ["-hls_segment_type", "fmp4"]
            if name == "fmp4-switch":
                args += [
                    "-streamid",
                    "0:11",
                    "-streamid",
                    "1:12",
                    "-hls_segment_options",
                    "use_stream_ids_as_track_ids=1",
                ]
            if name == "byterange":
                args += ["-hls_flags", "single_file"]
            if name == "encrypted":
                key = directory / "key.bin"
                key.write_bytes(bytes(range(16)))
                key_info = directory / "key.txt"
                key_info.write_text(f"key.bin\n{key}\n")
                args += ["-hls_key_info_file", str(key_info)]
            args += [(directory / "index.m3u8").as_posix()]
        command(*args)
    numbered = run.fixtures / "hls-numbered"
    numbered.mkdir()
    command(
        ffmpeg,
        "-v",
        "error",
        "-i",
        str(source),
        "-map",
        "0",
        "-c",
        "copy",
        "-f",
        "hls",
        "-hls_time",
        "2",
        "-hls_list_size",
        "0",
        "-hls_segment_filename",
        (numbered / "part%d_7.ts").as_posix(),
        (numbered / "index.m3u8").as_posix(),
    )
    hls_manifest = run.fixtures / "hls/index.m3u8"
    hls_manifest.write_text(
        hls_manifest.read_text(encoding="utf-8").replace(
            "#EXT-X-MEDIA-SEQUENCE:0", "#EXT-X-MEDIA-SEQUENCE:3456"
        )
    )
    openssl = shutil.which("openssl")
    if not openssl:
        raise RuntimeError("OpenSSL is required for the encrypted fMP4 fixtures")
    encrypted_fmp4 = run.fixtures / "encrypted-fmp4"
    encrypted_fmp4.mkdir()
    key = bytes(range(16))
    (encrypted_fmp4 / "key.bin").write_bytes(key)
    for media in (run.fixtures / "fmp4").iterdir():
        if media.suffix not in (".mp4", ".m4s"):
            continue
        command(
            openssl,
            "enc",
            "-aes-128-cbc",
            "-K",
            key.hex(),
            "-iv",
            "00" * 16,
            "-in",
            str(media),
            "-out",
            str(encrypted_fmp4 / media.name),
        )
    key_tag = f'#EXT-X-KEY:URI="key.bin",IV=0x{"00" * 16},METHOD=AES-128\n'
    for name in ("encrypted-init", "clear-init"):
        initialization = "init.mp4" if name == "encrypted-init" else "../fmp4/init.mp4"
        mapping = f'#EXT-X-MAP:URI="{initialization}"\n'
        tags = key_tag + mapping if name == "encrypted-init" else mapping + key_tag
        (encrypted_fmp4 / f"{name}.m3u8").write_text(
            "#EXTM3U\n#EXT-X-VERSION:7\n#EXT-X-TARGETDURATION:2\n"
            + tags
            + "".join(f"#EXTINF:2,\nindex{index}.m4s\n" for index in range(3))
            + "#EXT-X-ENDLIST\n"
        )
    switched = ["#EXTM3U", "#EXT-X-VERSION:7", "#EXT-X-TARGETDURATION:2"]
    for name in ("fmp4", "fmp4-switch"):
        if name == "fmp4-switch":
            switched.append("#EXT-X-DISCONTINUITY")
        switched.append(f'#EXT-X-MAP:URI="{name}/init.mp4"')
        for index in range(3):
            switched += ["#EXTINF:2,", f"{name}/index{index}.m4s"]
    (run.fixtures / "switch.m3u8").write_text(
        "\n".join(switched + ["#EXT-X-ENDLIST", ""])
    )
    manifest = ET.parse(run.fixtures / "dash/index.mpd")
    ns = "{urn:mpeg:dash:schema:mpd:2011}"
    root = manifest.getroot()
    root.set("mediaPresentationDuration", "PT12S")
    period = root.find(ns + "Period")
    period.set("duration", "PT6S")
    second = copy.deepcopy(period)
    second.set("id", "1")
    second.set("start", "PT6S")
    root.append(second)
    ET.register_namespace("", ns[1:-1])
    manifest.write(
        run.fixtures / "dash/multi.mpd", encoding="utf-8", xml_declaration=True
    )
    clipped = copy.deepcopy(root)
    clipped.set("mediaPresentationDuration", "PT10S")
    for index, item in enumerate(clipped.findall(ns + "Period")):
        item.set("start", f"PT{index * 5}S")
        item.set("duration", "PT5S")
    ET.ElementTree(clipped).write(run.fixtures / "dash/clipped.mpd")
    higher = run.fixtures / "higher"
    higher.mkdir()
    command(
        ffmpeg,
        "-v",
        "error",
        "-i",
        str(source),
        "-f",
        "lavfi",
        "-i",
        "sine=frequency=880:sample_rate=48000",
        "-t",
        "6",
        "-map",
        "0:v",
        "-map",
        "1:a",
        "-vf",
        "scale=320:180",
        "-c:v",
        "libx264",
        "-preset",
        "ultrafast",
        "-g",
        "30",
        "-c:a",
        "aac",
        "-f",
        "hls",
        "-hls_time",
        "2",
        "-hls_list_size",
        "0",
        (higher / "index.m3u8").as_posix(),
    )
    for language, audio_source in (("en", source), ("fr", higher / "index.m3u8")):
        audio_dir = run.fixtures / f"audio-{language}"
        audio_dir.mkdir()
        if language == "fr":
            timestamp = "".join(
                f"\\x{byte:02x}" for byte in (126000).to_bytes(8, "big")
            )
            command(
                ffmpeg,
                "-v",
                "error",
                "-i",
                audio_source.as_posix(),
                "-map",
                "0:a",
                "-c",
                "copy",
                "-write_id3v2",
                "1",
                "-metadata",
                f"id3v2_priv.com.apple.streaming.transportStreamTimestamp={timestamp}",
                "-f",
                "adts",
                (audio_dir / "audio.aac").as_posix(),
            )
            (audio_dir / "index.m3u8").write_text(
                "#EXTM3U\n#EXT-X-TARGETDURATION:7\n#EXTINF:6.04,\naudio.aac\n#EXT-X-ENDLIST\n"
            )
            continue
        command(
            ffmpeg,
            "-v",
            "error",
            "-i",
            audio_source.as_posix(),
            "-map",
            "0:a",
            "-c",
            "copy",
            "-f",
            "hls",
            "-hls_time",
            "2",
            "-hls_list_size",
            "0",
            (audio_dir / "index.m3u8").as_posix(),
        )
    (run.fixtures / "quality.m3u8").write_text(
        '#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",NAME="English",LANGUAGE="en",URI="audio-en/index.m3u8"\n'
        '#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",NAME="French",LANGUAGE="fr",URI="audio-fr/index.m3u8"\n'
        '#EXT-X-STREAM-INF:BANDWIDTH=400000,RESOLUTION=160x90,CODECS="avc1.42c00b,mp4a.40.2",AUDIO="audio"\nhls/index.m3u8\n'
        '#EXT-X-STREAM-INF:BANDWIDTH=800000,RESOLUTION=320x180,CODECS="avc1.42c00b,mp4a.40.2",AUDIO="audio"\nhigher/index.m3u8\n'
    )
    offset_root = ET.parse(run.fixtures / "dash/index.mpd").getroot()
    offset_period = offset_root.find(ns + "Period")
    offset_period.clear()
    offset_period.set("start", "PT0S")
    for index, (kind, offset) in enumerate((("v", 5), ("a", 9))):
        name = f"offset-{kind}"
        directory = run.fixtures / name
        directory.mkdir()
        command(
            ffmpeg,
            "-v",
            "error",
            "-i",
            str(source),
            "-map",
            f"0:{kind}",
            "-c",
            "copy",
            "-output_ts_offset",
            str(offset),
            "-format_options",
            "movie_timescale=48000",
            "-f",
            "dash",
            "-seg_duration",
            "2",
            (directory / "index.mpd").as_posix(),
        )
        adaptation = (
            ET.parse(directory / "index.mpd")
            .getroot()
            .find(f"{ns}Period/{ns}AdaptationSet")
        )
        adaptation.set("id", str(index))
        base = ET.Element(ns + "BaseURL")
        base.text = f"{name}/"
        adaptation.insert(0, base)
        for template in adaptation.iter(ns + "SegmentTemplate"):
            template.set(
                "presentationTimeOffset",
                str(offset * int(template.get("timescale", "1"))),
            )
        offset_period.append(adaptation)
    ET.ElementTree(offset_root).write(
        run.fixtures / "offset.mpd", encoding="utf-8", xml_declaration=True
    )
    expected = decoded_hash(ffmpeg, str(source), "v")
    expected_audio = decoded_hash(ffmpeg, str(source), "a")
    results = {}
    subtitle_root = run.fixtures / "hls"
    (subtitle_root / "subtitles.m3u8").write_text(
        "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:2\n"
        + "".join(f"#EXTINF:2,\nsub{i}.vtt\n" for i in range(3))
        + "#EXT-X-ENDLIST\n"
    )
    for i in range(3):
        (subtitle_root / f"sub{i}.vtt").write_text(
            f"WEBVTT\nX-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:{126000 + i * 180000}\n"
            f"\n00:00:00.000 --> 00:00:01.500\nCaption {i}\n\n"
        )
    (subtitle_root / "master.m3u8").write_text(
        '#EXTM3U\n#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID="subs",NAME="English",LANGUAGE="en",URI="subtitles.m3u8"\n'
        '#EXT-X-STREAM-INF:BANDWIDTH=400000,RESOLUTION=160x90,CODECS="avc1.42c00b,mp4a.40.2",SUBTITLES="subs"\nindex.m3u8\n'
    )
    with (
        CaddyService(run, "media", run.fixtures) as server,
        EngineProcess(run, "media", engine_path) as engine,
    ):
        clear_audio = decoded_hash(
            ffmpeg, (run.fixtures / "fmp4/index.m3u8").as_posix(), "a", 6
        )
        for name in ("encrypted-init", "clear-init"):
            gid = engine.add_uri(
                f"{server.base_url}/encrypted-fmp4/{name}.m3u8", {"out": f"{name}.mp4"}
            )
            output = wait(engine, gid)["files"][0]["path"]
            assert decoded_hash(ffmpeg, output) == expected, name
            assert decoded_hash(ffmpeg, output, "a", 6) == clear_audio, name
        results["encryptedFmp4"] = (
            "key attribute order and independent initialization encryption"
        )
        for name in ("skewed-live", "audio-late"):
            skewed = run.fixtures / name
            skewed.mkdir()
            for kind, length in (
                ("v", "2" if name == "skewed-live" else "10"),
                ("a", "10"),
            ):
                directory = skewed / kind
                directory.mkdir()
                manifest = directory / "index.m3u8"
                command(
                    ffmpeg,
                    "-v",
                    "error",
                    "-i",
                    str(source),
                    "-map",
                    f"0:{kind}",
                    "-c",
                    "copy",
                    "-output_ts_offset",
                    "4" if name == "audio-late" and kind == "a" else "0",
                    "-f",
                    "hls",
                    "-hls_time",
                    length,
                    "-hls_list_size",
                    "0",
                    "-hls_segment_type",
                    "fmp4",
                    manifest.as_posix(),
                )
                manifest.write_text(
                    manifest.read_text(encoding="utf-8").replace("#EXT-X-ENDLIST", ""),
                    encoding="utf-8",
                )
            (skewed / "master.m3u8").write_text(
                '#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",NAME="Audio",URI="a/index.m3u8"\n'
                '#EXT-X-STREAM-INF:BANDWIDTH=400000,RESOLUTION=160x90,CODECS="avc1.42c00b,mp4a.40.2",AUDIO="audio"\nv/index.m3u8\n',
                encoding="utf-8",
            )
            for container in ("mp4", "mkv"):
                gid = engine.add_uri(
                    f"{server.base_url}/{name}/master.m3u8",
                    {
                        "out": f"{name}.{container}",
                        "media-format": container,
                        "media-record-time": "2",
                    },
                )
                status = wait(engine, gid)
                output = status["files"][0]["path"]
                info = probe(ffprobe, output, "-show_streams", "-show_format")
                assert abs(float(info["format"]["duration"]) - 2) < 0.1, info
                assert all(
                    abs(float(s["start_time"])) < 0.1 for s in info["streams"]
                ), info
                assert 2000 <= int(status["media"]["completedDuration"]) < 2100, status
                decoded_hash(ffmpeg, output, "v")
                decoded_hash(ffmpeg, output, "a")
        results["liveAlignment"] = (
            "Independent playlist edges share one recording window"
        )
        for codec, encoder, suffix, container in (
            ("mp4a.6B", "libmp3lame", "mp3", "mp4"),
            ("vorbis", "libvorbis", "webm", "mkv"),
        ):
            encoded = run.fixtures / f"audio-{encoder}.{suffix}"
            command(
                ffmpeg,
                "-v",
                "error",
                "-i",
                str(source),
                "-map",
                "0:a",
                "-c:a",
                encoder,
                str(encoded),
            )
            timing = probe(ffprobe, str(encoded), "-show_format")["format"]
            offset = round(float(timing.get("start_time", 0)) * 1000000)
            manifest = run.fixtures / f"{encoder}.mpd"
            mime = "audio/mpeg" if suffix == "mp3" else "audio/webm"
            manifest.write_text(
                f'<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" mediaPresentationDuration="PT6S">'
                f'<Period duration="PT6S"><AdaptationSet mimeType="{mime}" codecs="{codec}">'
                '<Representation id="audio" bandwidth="128000">'
                f'<SegmentList timescale="1000000" duration="6000000" presentationTimeOffset="{offset}">'
                f'<SegmentURL media="{encoded.name}"/></SegmentList>'
                "</Representation></AdaptationSet></Period></MPD>",
                encoding="utf-8",
            )
            gid = engine.add_uri(
                f"{server.base_url}/{manifest.name}",
                {
                    "out": f"{encoder}.{container}",
                    "media-video": "none",
                    "media-format": container,
                },
            )
            output = wait(engine, gid)["files"][0]["path"]
            assert decoded_hash(ffmpeg, output, "a", 6) == decoded_hash(
                ffmpeg, str(encoded), "a", 6
            ), codec
            results[encoder] = "native audio remux preserves decoded samples"
        for name, suffix, container in (
            ("hls", "m3u8", "mp4"),
            ("fmp4", "m3u8", "mkv"),
            ("dash", "mpd", "mp4"),
            ("byterange", "m3u8", "mp4"),
            ("encrypted", "m3u8", "mp4"),
        ):
            gid = engine.add_uri(
                f"{server.base_url}/{name}/index.{suffix}",
                {
                    "out": f"{name}.{container}",
                    "media-format": container,
                    "max-tries": "2",
                },
            )
            status = wait(engine, gid)
            output = Path(status["files"][0]["path"])
            actual = decoded_hash(ffmpeg, str(output), "v")
            assert actual == expected, (name, actual, expected)
            info = probe(ffprobe, str(output), "-show_streams", "-show_format")
            assert {s["codec_type"] for s in info["streams"]} == {
                "audio",
                "video",
            }, info
            assert abs(float(info["format"]["duration"]) - 6) < 0.2, info
            assert status["media"]["state"] == "complete", status
            assert status["media"]["progress"] == "1.000000", status
            assert int(status["completedLength"]) == output.stat().st_size, status
            results[name] = {
                "bytes": output.stat().st_size,
                "duration": info["format"]["duration"],
                "videoHash": actual,
            }
        gid = engine.add_uri(f"{server.base_url}/dash/multi.mpd", {"out": "multi.mp4"})
        status = wait(engine, gid)
        info = probe(ffprobe, status["files"][0]["path"], "-show_format")
        assert abs(float(info["format"]["duration"]) - 12) < 0.2, info
        results["multiPeriod"] = info["format"]["duration"]

        gid = engine.add_uri(
            f"{server.base_url}/dash/clipped.mpd", {"out": "clipped.mp4"}
        )
        status = wait(engine, gid)
        info = probe(
            ffprobe,
            status["files"][0]["path"],
            "-count_frames",
            "-show_entries",
            "stream=codec_type,nb_read_frames",
            "-show_format",
        )
        assert abs(float(info["format"]["duration"]) - 10) < 0.1, info
        assert (
            next(s for s in info["streams"] if s["codec_type"] == "video")[
                "nb_read_frames"
            ]
            == "150"
        ), info
        decoded_hash(ffmpeg, status["files"][0]["path"], "a")
        results["periodBoundaries"] = (
            "overhanging fragments clipped to the declared periods"
        )

        gid = engine.add_uri(
            f"{server.base_url}/quality.m3u8",
            {"out": "quality.mp4", "media-pause-after-probe": "true"},
        )
        tracks = wait(engine, gid, "paused")["media"]["tracks"]
        assert {t["language"] for t in tracks if t["type"] == "audio"} == {
            "en",
            "fr",
        }, tracks
        quality_manifest = run.fixtures / "quality.m3u8"
        original_quality = quality_manifest.read_text(encoding="utf-8")
        lines = original_quality.splitlines()
        quality_manifest.write_text("\n".join([lines[index] for index in [0, 2, 1, 5, 6, 3, 4]]) + "\n", encoding="utf-8")
        reordered_gid = engine.add_uri(f"{server.base_url}/quality.m3u8", {
            "out": "reordered.mp4", "media-pause-after-probe": "true",
        })
        reordered_tracks = wait(engine, reordered_gid, "paused")["media"]["tracks"]
        assert {track["id"] for track in tracks} == {track["id"] for track in reordered_tracks}, reordered_tracks
        quality_manifest.write_text(original_quality, encoding="utf-8")
        control_action(engine, reordered_gid, "remove-force")
        results["trackIdentity"] = "HLS rendition and quality reordering preserves track IDs"
        low = next(t["id"] for t in tracks if t["height"] == "90")
        engine.rpc.call(
            "aria2.changeOption",
            [
                gid,
                {
                    "media-video": low,
                    "media-audio": "fr",
                    "media-pause-after-probe": "false",
                },
            ],
        )
        engine.rpc.call("aria2.unpause", [gid])
        output = wait(engine, gid)["files"][0]["path"]
        assert decoded_hash(ffmpeg, output) == expected
        assert decoded_hash(ffmpeg, output, "a") == decoded_hash(
            ffmpeg, (higher / "index.m3u8").as_posix(), "a"
        )
        results["initialSelection"] = (
            "selected video and language applied before the first media packet"
        )

        gid = engine.add_uri(f"{server.base_url}/switch.m3u8", {"out": "switch.mp4"})
        status = wait(engine, gid)
        switched_info = probe(
            ffprobe,
            status["files"][0]["path"],
            "-count_frames",
            "-show_entries",
            "stream=codec_type,nb_read_frames",
            "-show_format",
        )
        assert abs(float(switched_info["format"]["duration"]) - 12) < 0.2, switched_info
        assert (
            next(s for s in switched_info["streams"] if s["codec_type"] == "video")[
                "nb_read_frames"
            ]
            == "180"
        ), switched_info
        results["initializationSwitch"] = (
            "both initialization sections and 180 video frames retained"
        )

        gid = engine.add_uri(f"{server.base_url}/offset.mpd", {"out": "offset.mp4"})
        status = wait(engine, gid)
        for kind, expected_hash in (("v", expected), ("a", expected_audio)):
            actual = decoded_hash(ffmpeg, status["files"][0]["path"], kind)
            assert actual == expected_hash, (kind, actual, expected_hash)
        streams = probe(ffprobe, status["files"][0]["path"], "-show_streams")["streams"]
        assert all(abs(float(stream["start_time"])) < 0.05 for stream in streams), (
            streams
        )
        results["presentationOffsets"] = (
            "different audio/video offsets removed; both decoded hashes match"
        )

        gid = engine.add_uri(
            f"{server.base_url}/dash/index.mpd",
            {
                "out": "audio.mkv",
                "media-format": "mkv",
                "media-pause-after-probe": "true",
            },
        )
        paused = wait(engine, gid, "paused")
        assert paused["media"]["state"] == "awaiting-selection", paused
        engine.rpc.call(
            "aria2.changeOption",
            [gid, {"media-video": "none", "media-pause-after-probe": "false"}],
        )
        engine.rpc.call("aria2.unpause", [gid])
        status = wait(engine, gid)
        info = probe(ffprobe, status["files"][0]["path"], "-show_streams")
        assert {s["codec_type"] for s in info["streams"]} == {"audio"}, info
        results["selection"] = "audio only"

        # GPAC infers a start number from digits in segment names; it once
        # shifted every later segment time by it and ended ranges too early.
        gid = engine.add_uri(
            f"{server.base_url}/hls-numbered/index.m3u8",
            {"out": "range.mp4", "media-end-time": "4"},
        )
        status = wait(engine, gid)
        info = probe(ffprobe, status["files"][0]["path"], "-show_format")
        assert 3.5 <= float(info["format"]["duration"]) <= 4.5, info
        # Segments fetched ahead of the range end must not keep the
        # recovery directory open.
        assert not (engine.root / "media/tasks" / gid).exists(), gid
        results["rangeEnd"] = (
            "an HLS range ends at its end time and leaves no recovery data"
        )

        gid = engine.add_uri(
            f"{server.base_url}/hls/master.m3u8",
            {"out": "subtitles.mkv", "media-format": "mkv", "media-subtitles": "en"},
        )
        status = wait(engine, gid)
        captions = command(
            ffmpeg,
            "-v",
            "error",
            "-i",
            status["files"][0]["path"],
            "-map",
            "0:s",
            "-f",
            "webvtt",
            "-",
        )
        assert all(f"Caption {i}" in captions for i in range(3)), captions
        packets = probe(
            ffprobe,
            status["files"][0]["path"],
            "-select_streams",
            "s",
            "-show_entries",
            "packet=pts_time",
        )["packets"]
        assert len(packets) == 3 and all(
            abs(float(packet["pts_time"]) - index * 2) < 0.1
            for index, packet in enumerate(packets)
        ), packets
        results["subtitles"] = "caption content and mapped timestamps retained"

        gid = engine.add_uri(
            f"{server.base_url}/hls/master.m3u8",
            {
                "out": "missing-language.mkv",
                "media-format": "mkv",
                "media-subtitles": "missing",
            },
        )
        status = wait(engine, gid, "error")
        assert "unavailable" in status["media"]["error"], status
        assert not Path(status["files"][0]["path"]).exists(), status
        engine.rpc.call("aria2.removeDownloadResult", [gid])

        blocked_directory = run.fixtures / "blocked-directory"
        blocked_directory.write_text("regular file")
        gid = engine.add_uri(
            f"{server.base_url}/hls/index.m3u8",
            {"dir": str(blocked_directory), "out": "blocked.mp4"},
        )
        status = wait(engine, gid, "error")
        assert "Media file operation failed" in status["errorMessage"], status
        engine.rpc.call("aria2.removeDownloadResult", [gid])
        results["selectionAndFileErrors"] = (
            "missing language rejected; native filesystem errors remain valid RPC JSON"
        )

        gid = engine.add_uri(
            f"{server.base_url}/hls/index.m3u8",
            {"out": "hls-audio.mkv", "media-format": "mkv", "media-video": "none"},
        )
        status = wait(engine, gid)
        info = probe(ffprobe, status["files"][0]["path"], "-show_streams")
        assert {s["codec_type"] for s in info["streams"]} == {"audio"}, info

        # Native servers supply fault responses; the engine must never publish
        # a successful partial presentation or silently fall back to a file.
        with WireMockService(run, "media-faults") as faults:
            results["epochTimeline"] = validate_epoch_timeline(
                run, source, ffmpeg, ffprobe, engine, server, faults
            )
            with live_source(
                run, source, ffmpeg, "dash", "empty-dash", duration=24
            ) as manifest:
                empty = ET.parse(run.fixtures / manifest).getroot()
                for timeline in empty.findall(".//{*}SegmentTimeline"):
                    timeline.clear()
                for state in ("Started", "ready"):
                    faults.stub(
                        {
                            "scenarioName": "empty-dash",
                            "requiredScenarioState": state,
                            "newScenarioState": "ready",
                            "request": {"method": "GET", "urlPath": f"/{manifest}"},
                            "response": {
                                "status": 200,
                                "body": ET.tostring(empty, encoding="unicode"),
                            }
                            if state == "Started"
                            else {"proxyBaseUrl": server.base_url},
                        }
                    )
                faults.stub(
                    {
                        "priority": 10,
                        "request": {
                            "method": "GET",
                            "urlPathPattern": "/empty-dash/.*",
                        },
                        "response": {"proxyBaseUrl": server.base_url},
                    }
                )
                gid = engine.add_uri(
                    f"{faults.base_url}/{manifest}",
                    {"out": "empty-dash.mp4", "media-record-time": "16"},
                )
                status = wait(engine, gid, timeout=40)
                output = status["files"][0]["path"]
                assert (
                    float(probe(ffprobe, output, "-show_format")["format"]["duration"])
                    >= 15.9
                ), status
                command(
                    ffmpeg, "-v", "error", "-xerror", "-i", output, "-f", "null", "-"
                )
                results["emptyLiveTimeline"] = (
                    "an initially empty DASH timeline grows without false end-of-stream"
                )

            headers = {
                "Authorization": {"equalTo": "Bearer media-test"},
                "Cookie": {"equalTo": "session=media-test"},
                "Referer": {"equalTo": "https://player.example/"},
            }
            authenticated = ["#EXTM3U", "#EXT-X-TARGETDURATION:2"]
            for index in range(3):
                route = f"/auth/index{index}.ts"
                expected_headers = headers
                if index == 1:
                    route = "/foreign.ts"
                    expected_headers = {
                        **headers,
                        "Authorization": {"absent": True},
                        "Cookie": {"absent": True},
                    }
                faults.file(f"auth{index}.ts", run.fixtures / f"hls/index{index}.ts")
                faults.stub(
                    {
                        "request": {
                            "method": "GET",
                            "urlPath": route,
                            "headers": expected_headers,
                        },
                        "response": {"status": 200, "bodyFileName": f"auth{index}.ts"},
                    }
                )
                uri = f"http://localhost:{faults.port}{route}" if index == 1 else route
                authenticated += ["#EXTINF:2,", uri]
            faults.stub(
                {
                    "request": {
                        "method": "GET",
                        "urlPath": "/auth/manifest",
                        "headers": headers,
                    },
                    "response": {
                        "status": 200,
                        "body": "\n".join(authenticated + ["#EXT-X-ENDLIST", ""]),
                    },
                }
            )
            gid = engine.add_uri(
                f"{faults.base_url}/auth/manifest",
                {
                    "media": "hls",
                    "out": "authenticated.mp4",
                    "disable-ipv6": "true",
                    "header": [
                        "Authorization: Bearer media-test",
                        "Cookie: session=media-test",
                    ],
                    "referer": "https://player.example/",
                },
            )
            output = wait(engine, gid)["files"][0]["path"]
            assert decoded_hash(ffmpeg, output) == expected, output
            results["requestContext"] = (
                "credentials reach the origin and are stripped from a foreign origin"
            )

            for foreign_context in (False, True):
                prefix = "/scoped-" + ("two" if foreign_context else "one")
                origin = faults.base_url
                foreign = f"http://localhost:{faults.port}"
                source_headers = {"X-Media-Token": {"equalTo": "source"}}
                foreign_headers = {"X-Media-Token": {"equalTo": "foreign"} if foreign_context else {"absent": True}}
                faults.stub({"request": {"method":"GET", "urlPath":prefix + "/start", "headers":source_headers},
                             "response": {"status":302, "headers":{"Location":foreign + prefix + "/master.m3u8"}}})
                faults.stub({"request": {"method":"GET", "urlPath":prefix + "/master.m3u8", "headers":foreign_headers},
                             "response": {"status":200, "body":'#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=400000,RESOLUTION=160x90,CODECS="avc1.42c00b,mp4a.40.2"\n' + origin + prefix + '/child.m3u8\n'}})
                media_lines = ["#EXTM3U", "#EXT-X-TARGETDURATION:2"]
                for index in range(3):
                    route = prefix + f"/segment{index}.ts"
                    faults.file(f"scoped{index}.ts", run.fixtures / f"hls/index{index}.ts")
                    faults.stub({"request": {"method":"GET", "urlPath":route, "headers":foreign_headers if index == 1 else source_headers},
                                 "response": {"status":200, "bodyFileName":f"scoped{index}.ts"}})
                    media_lines.extend(["#EXTINF:2,", (foreign if index == 1 else origin) + route])
                faults.stub({"request": {"method":"GET", "urlPath":prefix + "/child.m3u8", "headers":source_headers},
                             "response": {"status":200, "body":"\n".join(media_lines + ["#EXT-X-ENDLIST", ""])}})
                contexts = [{"url":origin, "headers":[{"name":"X-Media-Token","value":"source"}]}]
                if foreign_context:
                    contexts.append({"url":foreign,"headers":[{"name":"X-Media-Token","value":"foreign"}]})
                gid = engine.add_uri(origin + prefix + "/start", {
                    "media":"hls", "filename-hint":"Scoped title 1.5" + prefix,
                    "filename-hint-source":"title", "media-pause-after-probe":"true",
                    "media-request-contexts":json.dumps(contexts), "disable-ipv6":"true",
                })
                wait(engine, gid, "paused")
                assert post_json(faults.base_url + "/__admin/requests/count", {"urlPathPattern":prefix + "/segment.*"})["count"] == 0
                engine.rpc.call("aria2.changeOption", [gid, {"media-pause-after-probe":"false"}])
                engine.rpc.call("aria2.unpause", [gid])
                output = wait(engine, gid)["files"][0]["path"]
                assert decoded_hash(ffmpeg, output) == expected, output
                assert "1.5" in Path(output).name, output
            results["scopedRequestContexts"] = "Redirects, child manifests and segments obey exact-origin custom headers; probes transfer no payload"

            for mode in ("late", "missing"):
                route = f"/{mode}/manifest"
                prefix = "#EXTM3U\n#EXT-X-TARGETDURATION:2\n"
                fragments = [
                    f"#EXTINF:2,\n{faults.base_url}/{mode}/{i}.ts\n" for i in range(3)
                ]
                for state, body in (
                    ("Started", prefix + fragments[0]),
                    ("ready", prefix + "".join(fragments)),
                ):
                    faults.stub(
                        {
                            "scenarioName": f"{mode}-playlist",
                            "requiredScenarioState": state,
                            "request": {"method": "GET", "urlPath": route},
                            "response": {"status": 200, "body": body},
                        }
                    )
                for index in range(3):
                    request = {"method": "GET", "urlPath": f"/{mode}/{index}.ts"}
                    response = {"status": 200, "bodyFileName": f"auth{index}.ts"}
                    if index == 0:
                        for state in ("Started", "ready"):
                            faults.stub(
                                {
                                    "scenarioName": f"{mode}-playlist",
                                    "requiredScenarioState": state,
                                    "newScenarioState": "ready",
                                    "request": request,
                                    "response": response,
                                }
                            )
                    elif index == 1:
                        faults.stub(
                            {
                                "scenarioName": f"{mode}-delivery",
                                "requiredScenarioState": "Started",
                                "newScenarioState": "ready",
                                "request": request,
                                "response": {"status": 404},
                            }
                        )
                        faults.stub(
                            {
                                "scenarioName": f"{mode}-delivery",
                                "requiredScenarioState": "ready",
                                "request": request,
                                "response": response
                                if mode == "late"
                                else {"status": 404},
                            }
                        )
                    else:
                        faults.stub({"request": request, "response": response})
                gid = engine.add_uri(
                    f"{faults.base_url}{route}",
                    {"media": "hls", "out": f"{mode}.mp4", "media-record-time": "6"},
                )
                status = wait(engine, gid, "complete" if mode == "late" else "error")
                if mode == "late":
                    assert (
                        decoded_hash(ffmpeg, status["files"][0]["path"]) == expected
                    ), status
                else:
                    assert not Path(status["files"][0]["path"]).exists(), status
                    engine.rpc.call("aria2.removeDownloadResult", [gid])
            results["liveAvailability"] = (
                "native bounded wait recovers a late segment; permanent loss fails without output"
            )
            fragment = run.fixtures / "hls/index0.ts"
            size = fragment.stat().st_size
            faults.file("fragment.ts", fragment)
            faults.stub(
                {
                    "request": {"method": "GET", "urlPath": "/wrong.ts"},
                    "response": {
                        "status": 206,
                        "headers": {"Content-Range": f"bytes 1-{size}/{size + 1}"},
                        "bodyFileName": "fragment.ts",
                    },
                }
            )
            playlist = (run.fixtures / "hls/index.m3u8").read_text(encoding="utf-8")
            playlist = "\n".join(
                f"{server.base_url}/hls/{line}" if line.endswith(".ts") else line
                for line in playlist.splitlines()
            )
            for route, body in (
                ("manifest", playlist),
                ("missing", playlist.replace("index1.ts", "absent.ts")),
                (
                    "wrong-range",
                    f"#EXTM3U\n#EXT-X-TARGETDURATION:2\n#EXTINF:2,\n{faults.base_url}/wrong.ts\n#EXT-X-ENDLIST\n",
                ),
                (
                    "invalid",
                    "#EXTM3U\n#EXTINF:2,\nfile:///etc/passwd\n#EXT-X-ENDLIST\n",
                ),
            ):
                faults.stub(
                    {
                        "request": {"method": "GET", "urlPath": f"/{route}"},
                        "response": {
                            "status": 200,
                            "headers": {
                                "Content-Type": "application/vnd.apple.mpegurl"
                            },
                            "body": body,
                        },
                    }
                )
                gid = engine.add_uri(
                    f"{faults.base_url}/{route}",
                    {"out": f"{route}.mp4", "max-tries": "1"},
                )
                status = wait(
                    engine, gid, "complete" if route == "manifest" else "error"
                )
                if route == "manifest":
                    actual = decoded_hash(ffmpeg, status["files"][0]["path"], "v")
                    assert actual == expected, status
                else:
                    assert (
                        status["media"]["state"] == "error" and status["media"]["error"]
                    ), status
                    assert not Path(status["files"][0]["path"]).exists(), status
                    engine.rpc.call("aria2.removeDownloadResult", [gid])
                    assert not (engine.root / "media/tasks" / gid).exists(), gid
            live_playlist = playlist.replace("#EXT-X-ENDLIST", "")
            for index, state in enumerate(("Started", "loaded", "refresh")):
                faults.stub(
                    {
                        "scenarioName": "live-refresh-failure",
                        "requiredScenarioState": state,
                        **(
                            {"newScenarioState": ("loaded", "refresh")[index]}
                            if index < 2
                            else {}
                        ),
                        "request": {"method": "GET", "urlPath": "/refresh.m3u8"},
                        "response": {
                            "status": 200,
                            "headers": {
                                "Content-Type": "application/vnd.apple.mpegurl"
                            },
                            "body": live_playlist,
                        }
                        if index < 2
                        else {"status": 503},
                    }
                )
            gid = engine.add_uri(
                f"{faults.base_url}/refresh.m3u8",
                {"out": "refresh-failure.mp4", "max-tries": "1"},
            )
            status = wait(engine, gid, "error")
            assert "503" in status["media"]["error"], status
            assert not Path(status["files"][0]["path"]).exists(), status
            engine.rpc.call("aria2.removeDownloadResult", [gid])
            results["failureHandling"] = (
                "MIME detection succeeds; missing fragments and local-file references fail without publishing output"
            )

        for protocol in ("hls", "dash"):
            operations = {}
            with live_source(
                run, source, ffmpeg, protocol, f"live-{protocol}"
            ) as manifest:
                for action in (
                    "auto",
                    "finish",
                    "finish-paused",
                    "pause",
                    "force-pause",
                    "restart",
                    "crash",
                    "remove-active",
                    "remove-paused",
                    "remove-force",
                    "expired",
                ):
                    name = f"{protocol}-{action}"
                    options = {"out": f"{name}.mp4"}
                    if action == "auto":
                        options["media-record-time"] = "4"
                    gid = engine.add_uri(f"{server.base_url}/{manifest}", options)
                    before = wait_duration(engine, gid, 2000)
                    if action == "expired":
                        engine.rpc.call("aria2.pause", [gid])
                        wait(engine, gid, "paused")
                        time.sleep(13)
                        engine.rpc.call("aria2.unpause", [gid])
                        status = wait(engine, gid, "error")
                        assert any(
                            word in status["media"]["error"].lower()
                            for word in ("missing", "gap", "window")
                        ), status
                        assert not Path(status["files"][0]["path"]).exists(), status
                        engine.rpc.call("aria2.removeDownloadResult", [gid])
                        operations[action] = "expired recording rejected without output"
                        continue
                    if action != "auto":
                        evidence = control_action(engine, gid, action)
                        if action.startswith("remove-"):
                            operations[action] = evidence
                            continue
                        if action in ("pause", "force-pause", "restart", "crash"):
                            before = wait_duration(
                                engine,
                                gid,
                                int(before["media"]["completedDuration"]) + 2000,
                            )
                            control_action(engine, gid, "pause")
                            wait_duration(
                                engine,
                                gid,
                                int(before["media"]["completedDuration"]) + 2000,
                            )
                            control_action(engine, gid, "finish")
                    status = wait(engine, gid)
                    output = status["files"][0]["path"]
                    info = probe(ffprobe, output, "-show_format")
                    assert float(info["format"]["duration"]) > 1, info
                    for kind in ("v", "a"):
                        decoded_hash(ffmpeg, output, kind)
                    operations[action] = status["media"]["completedDuration"]
            with live_source(
                run, source, ffmpeg, protocol, f"{protocol}-natural-end", 14
            ) as manifest:
                gid = engine.add_uri(
                    f"{server.base_url}/{manifest}",
                    {"out": f"{protocol}-natural-end.mp4"},
                )
                status = wait(engine, gid, timeout=40)
                assert status["media"]["live"] == "true", status
                for kind in ("v", "a"):
                    decoded_hash(ffmpeg, status["files"][0]["path"], kind)
                operations["source-end"] = status["media"]["completedDuration"]
            results[f"{protocol}LiveControls"] = operations

        session = engine.root / "session.txt"
        engine.rpc.call("aria2.changeGlobalOption", [{"save-session": str(session)}])
        gid = engine.add_uri(
            f"{server.base_url}/hls/index.m3u8",
            {"out": "resumed.mp4", "max-download-limit": "32K"},
        )
        until = time.monotonic() + 20
        while time.monotonic() < until:
            status = engine.rpc.call("aria2.tellStatus", [gid])
            if int(status.get("media", {}).get("completedDuration", "0")) >= 2000:
                break
            time.sleep(0.1)
        engine.rpc.call("aria2.pause", [gid])
        paused = wait(engine, gid, "paused")
        assert int(paused["media"]["completedDuration"]) >= 2000, paused
        engine.rpc.call("aria2.saveSession")
        engine.stop()
        with sqlite3.connect(engine.root / "media/state.db") as db:
            assert (
                db.execute(
                    "SELECT MIN(number) FROM media_segments WHERE gid=?", (gid,)
                ).fetchone()[0]
                == 3456
            )
            cached = Path(
                db.execute(
                    "SELECT path FROM media_segments WHERE gid=? LIMIT 1", (gid,)
                ).fetchone()[0]
            )
        cached.write_bytes(b"damaged cached fragment")
        engine.start([f"--input-file={session}", f"--save-session={session}"])
        restored = wait(engine, gid, "paused")
        assert (
            restored["media"]["completedDuration"]
            == paused["media"]["completedDuration"]
        ), (paused, restored)
        assert restored["downloadSpeed"] == "0", restored
        engine.rpc.call("aria2.unpause", [gid])
        status = wait(engine, gid)
        actual = decoded_hash(ffmpeg, status["files"][0]["path"], "v")
        assert actual == expected, actual
        results["restart"] = (
            "paused progress retained; corrupted cache re-fetched; complete video hash matches"
        )

        for active in (False, True):
            gid = engine.add_uri(
                f"{server.base_url}/hls/index.m3u8",
                {
                    "out": f"removed-{active}.mp4",
                    "media-pause-after-probe": "false" if active else "true",
                    "max-download-limit": "16K",
                },
            )
            if active:
                wait(engine, gid, "active")
            else:
                wait(engine, gid, "paused")
            engine.rpc.call("aria2.remove", [gid])
            until = time.monotonic() + 5
            while True:
                tasks = engine.rpc.call("aria2.tellActive") + engine.rpc.call(
                    "aria2.tellWaiting", [0, 100]
                )
                if not any(task["gid"] == gid for task in tasks):
                    break
                if time.monotonic() >= until:
                    raise TimeoutError(f"Media removal did not finish: {gid}")
                time.sleep(0.1)
            assert not (engine.root / "media/tasks" / gid).exists(), gid
            with sqlite3.connect(engine.root / "media/state.db") as db:
                for table in (
                    "media_tasks",
                    "media_segments",
                    "media_tracks",
                    "media_identity",
                    "media_manifests",
                    "media_selections",
                    "media_publication",
                ):
                    assert (
                        db.execute(
                            f"SELECT COUNT(*) FROM {table} WHERE gid=?", (gid,)
                        ).fetchone()[0]
                        == 0
                    ), (table, gid)
        results["removal"] = (
            "active and paused tasks discard only their own recovery data"
        )
    return results


if __name__ == "__main__":
    raise SystemExit(run_validation("media", validate))
