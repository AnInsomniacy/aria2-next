#!/usr/bin/env python3

from __future__ import annotations

import http.server
import re
import subprocess
import sys
import threading
import time
from pathlib import Path


SUITE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SUITE_ROOT))

from core.engine import EngineProcess
from core.report import run_validation
from core.runtime import BUILD_ROOT, REPOSITORY_ROOT, RunDirectory, create_payload, sha256


class WebSeedServer:
    def __init__(self, payload: bytes):
        self.payload = payload
        self.requests: list[tuple[str, dict[str, str]]] = []
        self.cross_origin_target = ""
        owner = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, _format: str, *_arguments: object) -> None:
                return

            def do_GET(self) -> None:
                owner.requests.append(
                    (
                        self.path,
                        {
                            key.lower(): value
                            for key, value in self.headers.items()
                        },
                    )
                )
                if self.path == "/same-origin":
                    self.send_response(302)
                    self.send_header("Location", "/payload.bin")
                    self.end_headers()
                    return
                if self.path == "/cross-origin":
                    self.send_response(302)
                    self.send_header("Location", owner.cross_origin_target)
                    self.end_headers()
                    return
                if self.path != "/payload.bin":
                    self.send_error(404)
                    return

                match = re.fullmatch(
                    r"bytes=(\d+)-(\d+)", self.headers.get("Range", "")
                )
                if match is None:
                    self.send_error(416)
                    return
                first, last = (int(value) for value in match.groups())
                body = owner.payload[first : last + 1]
                self.send_response(206)
                self.send_header("Accept-Ranges", "bytes")
                self.send_header(
                    "Content-Range", f"bytes {first}-{last}/{len(owner.payload)}"
                )
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    @property
    def base_url(self) -> str:
        return f"http://127.0.0.1:{self.server.server_port}"

    def start(self) -> None:
        self.thread.start()

    def stop(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)


def validate_web_seed_headers(
    run: RunDirectory,
    engine_path: Path | None,
    creator: Path,
    payload: Path,
    expected: str,
) -> dict[str, str]:
    payload_bytes = payload.read_bytes()
    origin = WebSeedServer(payload_bytes)
    foreign = WebSeedServer(payload_bytes)
    origin.cross_origin_target = f"{foreign.base_url}/payload.bin"
    origin.start()
    foreign.start()
    try:
        cases = [
            ("same-origin", f"{origin.base_url}/same-origin", True),
            ("cross-origin", f"{origin.base_url}/cross-origin", False),
        ]
        for name, web_seed, expect_credentials in cases:
            torrent = run.fixtures / f"{name}.torrent"
            subprocess.run(
                [str(creator), str(payload), str(torrent), web_seed], check=True
            )
            engine = EngineProcess(run, name, engine_path)
            engine.start(["--bt-interface=127.0.0.1", "--disable-ipv6=true"])
            try:
                gid = engine.add_torrent(
                    torrent,
                    {
                        "dir": str(engine.download_dir),
                        "seed-time": "0",
                        "header": [
                            "Authorization: Bearer private",
                            "Cookie: session=private",
                            "X-Aria2-Next: retained",
                        ],
                    },
                )
                engine.rpc.wait_content_complete(gid, 45)
            finally:
                engine.stop()

            downloaded = engine.download_dir / payload.name
            if sha256(downloaded) != expected:
                raise RuntimeError(f"{name} web-seed payload digest mismatch")

            target_server = origin if expect_credentials else foreign
            headers = next(
                (
                    headers
                    for path, headers in reversed(target_server.requests)
                    if path == "/payload.bin"
                ),
                None,
            )
            if headers is None:
                raise RuntimeError(f"{name} web seed received no payload request")
            if headers.get("x-aria2-next") != "retained":
                raise RuntimeError(f"{name} lost its non-sensitive custom header")
            expected_credentials = {
                "authorization": "Bearer private",
                "cookie": "session=private",
            }
            for header, value in expected_credentials.items():
                if expect_credentials and headers.get(header) != value:
                    raise RuntimeError(f"{name} lost its {header} header")
                if not expect_credentials and header in headers:
                    raise RuntimeError(f"{name} leaked its {header} header")
    finally:
        foreign.stop()
        origin.stop()

    return {
        "sameOriginHeaders": "preserved",
        "crossOriginCredentials": "removed",
        "crossOriginOtherHeaders": "preserved",
    }


def build_creator() -> Path:
    source = Path(__file__).resolve().parent
    build = BUILD_ROOT / "helpers" / "bittorrent"
    prefix = REPOSITORY_ROOT / "build/default/dependencies"
    boost = REPOSITORY_ROOT / "third_party/boost"
    subprocess.run(
        [
            "cmake",
            "-S",
            str(source),
            "-B",
            str(build),
            "-G",
            "Ninja",
            f"-DCMAKE_PREFIX_PATH={prefix}",
            f"-DBoost_INCLUDE_DIR={boost}",
            "-DBoost_NO_BOOST_CMAKE=ON",
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        check=True,
    )
    subprocess.run(["cmake", "--build", str(build)], check=True)
    return Path((build / "creator-path.txt").read_text(encoding="utf-8").strip())


def wait_seeding(engine: EngineProcess, gid: str, timeout: float) -> dict[str, object]:
    deadline = time.monotonic() + timeout
    last: dict[str, object] = {}
    while time.monotonic() < deadline:
        last = engine.rpc.call("aria2.tellStatus", [gid])
        bittorrent = last.get("bittorrent", {})
        if last.get("seeder") == "true" or (
            isinstance(bittorrent, dict) and bittorrent.get("state") == "seeding"
        ):
            return last
        time.sleep(0.1)
    raise TimeoutError(f"Seed task did not enter seeding state: {last}")


def validate(run: RunDirectory, engine_path: Path | None) -> dict[str, object]:
    seed = EngineProcess(run, "seed", engine_path)
    leecher = EngineProcess(run, "leecher", engine_path)
    payload = seed.download_dir / "payload.bin"
    expected = create_payload(payload, 4 * 1024 * 1024)
    torrent = run.fixtures / "payload.torrent"
    creator = build_creator()
    subprocess.run([str(creator), str(payload), str(torrent)], check=True)

    isolated_bt = ["--bt-interface=127.0.0.1", "--disable-ipv6=true"]
    seed.start(isolated_bt)
    leecher.start(isolated_bt)
    try:
        common = {
            "auto-file-renaming": "false",
            "allow-overwrite": "true",
            "check-integrity": "true",
            "seed-time": "10",
        }
        seed_gid = seed.add_torrent(torrent, {**common, "dir": str(seed.download_dir)})
        seed.rpc.wait_content_complete(seed_gid, 45)
        seed_status = wait_seeding(seed, seed_gid, 15)
        seed_session = seed.rpc.call("aria2.getBtSessionStatus")
        endpoints = seed_session.get("listenEndpoints", [])
        if not endpoints:
            raise RuntimeError(f"BitTorrent seed has no listen endpoint: {seed_session}")
        seed_endpoint = str(endpoints[0])

        started = time.monotonic()
        leecher_gid = leecher.add_torrent(
            torrent, {**common, "dir": str(leecher.download_dir)}
        )
        peer_result = leecher.rpc.call(
            "aria2.addBtPeers",
            [leecher_gid, [seed_endpoint]],
        )
        try:
            leecher_status = leecher.rpc.wait_content_complete(leecher_gid, 45)
        except Exception as error:
            raise RuntimeError(
                f"{error}; seedSession={seed_session}; "
                f"leecherSession={leecher.rpc.call('aria2.getBtSessionStatus')}"
            ) from error
        duration = round(time.monotonic() - started, 3)
        downloaded = leecher.download_dir / "payload.bin"
        if sha256(downloaded) != expected:
            raise RuntimeError("BitTorrent payload digest mismatch")
    finally:
        leecher.stop()
        seed.stop()

    web_seed_headers = validate_web_seed_headers(
        run, engine_path, creator, payload, expected
    )

    return {
        "sha256": expected,
        "bytes": payload.stat().st_size,
        "durationSeconds": duration,
        "peerResult": peer_result,
        "listenEndpoint": seed_endpoint,
        "seedStatus": seed_status.get("status"),
        "leecherStatus": leecher_status.get("status"),
        **web_seed_headers,
    }


if __name__ == "__main__":
    raise SystemExit(run_validation("bittorrent", validate))
