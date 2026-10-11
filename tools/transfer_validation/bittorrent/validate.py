#!/usr/bin/env python3
"""BitTorrent transfers between two engines on loopback.

Scenarios: a seed and a leecher, file selection in a multi-file torrent, a
magnet link that pauses for file selection, progress kept across pause and an
engine restart, and a .torrent fetched over HTTP that starts its download.
"""

from __future__ import annotations

import base64
import functools
import http.server
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

ISOLATED = ["--bt-interface=127.0.0.1", "--disable-ipv6=true"]
COMMON = {"auto-file-renaming": "false", "allow-overwrite": "true", "seed-time": "0"}
MIB = 1024 * 1024


def build_creator() -> Path:
    source = Path(__file__).resolve().parent
    build = BUILD_ROOT / "helpers" / "bittorrent"
    prefix = REPOSITORY_ROOT / "build/default/dependencies"
    boost = REPOSITORY_ROOT / "third_party/boost"
    subprocess.run(
        [
            "cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
            f"-DCMAKE_PREFIX_PATH={prefix}", f"-DBoost_INCLUDE_DIR={boost}",
            "-DBoost_NO_BOOST_CMAKE=ON", "-DCMAKE_BUILD_TYPE=Release",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    subprocess.run(["cmake", "--build", str(build)], check=True, stdout=subprocess.DEVNULL)
    return Path((build / "creator-path.txt").read_text(encoding="utf-8").strip())


def wait(engine: EngineProcess, gid: str, predicate, what: str, timeout: float = 30) -> dict:
    deadline = time.monotonic() + timeout
    last: dict = {}
    while time.monotonic() < deadline:
        last = engine.rpc.call("aria2.tellStatus", [gid])
        if last.get("status") == "error":
            raise RuntimeError(f"{gid} failed while waiting for {what}: {last.get('errorMessage')}")
        if predicate(last):
            return last
        time.sleep(0.1)
    raise TimeoutError(f"{gid} did not reach {what}: {last}")


def endpoint(seed: EngineProcess) -> str:
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        endpoints = seed.rpc.call("aria2.getBtSessionStatus").get("listenEndpoints", [])
        if endpoints:
            return str(endpoints[0])
        time.sleep(0.1)
    raise RuntimeError("The seed has no listen endpoint")


def seed_torrent(seed: EngineProcess, torrent: Path, options: dict | None = None) -> str:
    # Seeding stops at whichever of seed-time and seed-ratio comes first.
    gid = seed.add_torrent(torrent, {**COMMON, "seed-time": "60", "seed-ratio": "0",
                                     **(options or {})})
    wait(seed, gid, lambda s: s.get("seeder") == "true", "seeding")
    return gid


def selected_digests(directory: Path, names: list[str]) -> dict[str, str]:
    return {name: sha256(directory / name) for name in names}


def scenario_single(seed, leecher, fixtures, torrent, expected, peer) -> dict:
    started = time.monotonic()
    gid = leecher.add_torrent(torrent, COMMON)
    added = leecher.rpc.call("aria2.addBtPeers", [gid, [peer]])
    status = leecher.rpc.wait_complete(gid, 45)
    if sha256(leecher.download_dir / "payload.bin") != expected:
        raise RuntimeError("single: payload digest mismatch")
    return {"seconds": round(time.monotonic() - started, 3), "peers": added,
            "status": status["status"]}


def scenario_selection(seed, leecher, multi, digests, peer) -> dict:
    gid = leecher.add_torrent(multi, {**COMMON, "select-file": "2"})
    leecher.rpc.call("aria2.addBtPeers", [gid, [peer]])
    status = leecher.rpc.wait_complete(gid, 45)
    files = status["files"]
    if [f["selected"] for f in files] != ["false", "true", "false"]:
        raise RuntimeError(f"selection: unexpected selection {files}")
    if int(status["totalLength"]) != 3 * MIB:
        raise RuntimeError(f"selection: total length {status['totalLength']} is not the selection")
    got = sha256(leecher.download_dir / "multi" / "b.bin")
    if got != digests["b.bin"]:
        raise RuntimeError("selection: b.bin digest mismatch")
    return {"status": status["status"], "selectedLength": status["totalLength"]}


def scenario_magnet(seed, leecher, magnet, digests, peer) -> dict:
    gid = str(leecher.rpc.call("aria2.addUri", [[magnet], {**COMMON, "pause-metadata": "true",
                                                           "dir": str(leecher.download_dir / "magnet")}]))
    leecher.rpc.call("aria2.addBtPeers", [gid, [peer]])
    paused = wait(leecher, gid,
                  lambda s: s["status"] == "paused" and
                  s.get("bittorrent", {}).get("fileSelectionState") == "awaiting",
                  "the selection pause")
    if len(paused["files"]) != 3 or int(paused["completedLength"]) != 0:
        raise RuntimeError(f"magnet: unexpected files or data before selection: {paused}")
    leecher.rpc.call("aria2.changeOption", [gid, {"select-file": "1,3"}])
    leecher.rpc.call("aria2.unpause", [gid])
    leecher.rpc.call("aria2.addBtPeers", [gid, [peer]])
    status = leecher.rpc.wait_complete(gid, 45)
    root = leecher.download_dir / "magnet" / "multi"
    if selected_digests(root, ["a.bin", "c.bin"]) != {k: digests[k] for k in ("a.bin", "c.bin")}:
        raise RuntimeError("magnet: selected file digest mismatch")
    if (root / "b.bin").exists() and (root / "b.bin").stat().st_size == 3 * MIB:
        raise RuntimeError("magnet: an unselected file was downloaded")
    return {"status": status["status"], "selection": "1,3"}


def scenario_restart(seed, leecher, torrent, expected, peer, session: Path) -> dict:
    gid = leecher.add_torrent(torrent, {**COMMON, "dir": str(leecher.download_dir / "restart")})
    leecher.rpc.call("aria2.addBtPeers", [gid, [peer]])
    wait(leecher, gid, lambda s: int(s["completedLength"]) >= 2 * MIB, "partial progress")
    leecher.rpc.call("aria2.forcePause", [gid])
    paused = wait(leecher, gid, lambda s: s["status"] == "paused", "pause")
    before = int(paused["completedLength"])
    leecher.stop()
    leecher.start(ISOLATED + [f"--input-file={session}", f"--save-session={session}"])
    restored = wait(leecher, gid, lambda s: s["status"] == "paused", "the restored pause")
    after = int(restored["completedLength"])
    if after < before - 256 * 1024:
        raise RuntimeError(f"restart: progress fell from {before} to {after}")
    leecher.rpc.call("aria2.unpause", [gid])
    leecher.rpc.call("aria2.addBtPeers", [gid, [peer]])
    leecher.rpc.wait_complete(gid, 60)
    if sha256(leecher.download_dir / "restart" / "slow.bin") != expected:
        raise RuntimeError("restart: payload digest mismatch")
    return {"pausedAt": before, "restoredAt": after}


def scenario_follow(seed, leecher, fixtures, torrent, expected, peer) -> dict:
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(fixtures))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        url = f"http://127.0.0.1:{server.server_address[1]}/{torrent.name}"
        gid = str(leecher.rpc.call("aria2.addUri", [[url], {**COMMON,
                                                          "dir": str(leecher.download_dir / "follow")}]))
        done = leecher.rpc.wait_complete(gid, 30)
        followed = done.get("followedBy", [])
        if len(followed) != 1:
            raise RuntimeError(f"follow: no torrent task started: {done}")
        leecher.rpc.call("aria2.addBtPeers", [followed[0], [peer]])
        leecher.rpc.wait_complete(followed[0], 45)
    finally:
        server.shutdown()
    if sha256(leecher.download_dir / "follow" / "payload.bin") != expected:
        raise RuntimeError("follow: payload digest mismatch")
    return {"torrentTask": followed[0]}


def validate(run: RunDirectory, engine_path: Path | None) -> dict[str, object]:
    creator = build_creator()
    seed = EngineProcess(run, "seed", engine_path)
    leecher = EngineProcess(run, "leecher", engine_path)

    payload = seed.download_dir / "payload.bin"
    expected = create_payload(payload, 4 * MIB)
    torrent = run.fixtures / "payload.torrent"
    subprocess.run([str(creator), str(payload), str(torrent)], check=True)

    multi_root = seed.download_dir / "multi"
    digests = {name: create_payload(multi_root / name, size * MIB)
               for name, size in (("a.bin", 1), ("b.bin", 3), ("c.bin", 2))}
    multi = run.fixtures / "multi.torrent"
    subprocess.run([str(creator), str(multi_root), str(multi)], check=True)

    slow_payload = seed.download_dir / "slow.bin"
    slow_expected = create_payload(slow_payload, 8 * MIB)
    slow = run.fixtures / "slow.torrent"
    subprocess.run([str(creator), str(slow_payload), str(slow)], check=True)

    session = run.state / "leecher.session"
    seed.start(ISOLATED)
    leecher.start(ISOLATED + [f"--save-session={session}"])
    results: dict[str, object] = {}
    try:
        seed_torrent(seed, torrent)
        multi_gid = seed_torrent(seed, multi)
        seed_torrent(seed, slow, {"max-upload-limit": "1M"})
        peer = endpoint(seed)
        magnet = seed.rpc.call("aria2.tellStatus", [multi_gid])["bittorrent"]["magnetLink"]
        inspected = seed.rpc.call("aria2.inspectTorrent",
                                  [base64.b64encode(multi.read_bytes()).decode()])
        if [f["path"] for f in inspected["files"]] != ["multi/a.bin", "multi/b.bin", "multi/c.bin"]:
            raise RuntimeError(f"inspectTorrent: unexpected files {inspected}")

        results["single"] = scenario_single(seed, leecher, run.fixtures, torrent, expected, peer)
        results["selection"] = scenario_selection(seed, leecher, multi, digests, peer)
        results["magnet"] = scenario_magnet(seed, leecher, magnet, digests, peer)
        results["restart"] = scenario_restart(seed, leecher, slow, slow_expected, peer, session)
        results["follow"] = scenario_follow(seed, leecher, run.fixtures, torrent, expected, peer)
    finally:
        leecher.stop()
        seed.stop()
    return results


if __name__ == "__main__":
    raise SystemExit(run_validation("bittorrent", validate))
