#!/usr/bin/env python3

from __future__ import annotations

import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


SUITE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SUITE_ROOT))

from core.engine import EngineProcess
from core.report import run_validation
from core.runtime import (
    RunDirectory,
    create_payload,
    non_loopback_ipv4,
    process_options,
    sha256,
)


PART_SIZE = 9728000


def md4(data: bytes) -> bytes:
    openssl = shutil.which("openssl")
    if not openssl:
        raise RuntimeError("OpenSSL is required for ED2K fixture hashing")
    completed = subprocess.run(
        [openssl, "dgst", "-provider", "legacy", "-md4", "-r"],
        input=data,
        capture_output=True,
        check=True,
        **process_options(),
    )
    match = re.match(rb"([0-9a-fA-F]{32})", completed.stdout)
    if not match:
        raise RuntimeError("OpenSSL returned an invalid MD4 digest")
    return bytes.fromhex(match.group(1).decode())


def ed2k_hash(path: Path) -> str:
    """The root of the part hashes. A size that is an exact multiple of the
    part size has one more part, of nothing."""
    data = path.read_bytes()
    parts = [md4(data[at : at + PART_SIZE]) for at in range(0, len(data), PART_SIZE)]
    if len(data) % PART_SIZE == 0:
        parts.append(md4(b""))
    root = parts[0] if len(parts) == 1 else md4(b"".join(parts))
    return root.hex().upper()


def create_text_payload(path: Path, size: int) -> str:
    """Compressible content, as text and many documents are."""
    path.parent.mkdir(parents=True, exist_ok=True)
    line = 0
    written = 0
    with path.open("wb") as output:
        while written < size:
            chunk = b"".join(
                b"%08d the quick brown fox jumps over the lazy dog\n" % number
                for number in range(line, line + 4096)
            )[: size - written]
            line += 4096
            output.write(chunk)
            written += len(chunk)
    return sha256(path)


def wait_sharing(engine: EngineProcess, gid: str, timeout: float) -> dict[str, object]:
    deadline = time.monotonic() + timeout
    last: dict[str, object] = {}
    while time.monotonic() < deadline:
        last = engine.rpc.call("aria2.tellStatus", [gid])
        if last.get("seeder") == "true":
            return last
        time.sleep(0.1)
    raise TimeoutError(f"ED2K seed did not enter sharing state: {last}")


def wait_download(
    engine: EngineProcess, gid: str, timeout: float
) -> tuple[dict[str, object], int]:
    """Returns the final status and the most sources seen while downloading;
    sources are released once the file is complete."""
    deadline = time.monotonic() + timeout
    last: dict[str, object] = {}
    peers = 0
    while time.monotonic() < deadline:
        last = engine.rpc.call("aria2.tellStatus", [gid])
        ed2k = last.get("ed2k", {})
        if isinstance(ed2k, dict):
            peers = max(peers, int(str(ed2k.get("peerCount", "0"))))
        if last.get("status") == "error":
            raise RuntimeError(
                f"ED2K task failed: {last.get('errorCode')} {last.get('errorMessage')}"
            )
        total = int(str(last.get("totalLength", "0")))
        completed = int(str(last.get("completedLength", "0")))
        if total > 0 and completed == total:
            return last, peers
        time.sleep(0.1)
    ed2k = last.get("ed2k", {})
    if isinstance(ed2k, dict) and ed2k.get("peerCount") == "0":
        raise RuntimeError(
            "ED2K sources from the file link were not admitted to the peer queue"
        )
    raise TimeoutError(f"ED2K task did not complete: {last}")


def transfer(
    seed: EngineProcess,
    leecher: EngineProcess,
    name: str,
    expected: str,
    options: dict[str, str],
    timeout: float,
) -> dict[str, object]:
    """Shares the seed's file through an inline source and downloads it on the
    leecher. The links carry no part hashes: the seed derives them from the
    file, and the leecher asks the seed for them."""
    payload = seed.download_dir / name
    size = payload.stat().st_size
    root = ed2k_hash(payload)
    seed_link = f"ed2k://|file|{name}|{size}|{root}|sources,127.0.0.1:{seed.ed2k_port}|/"
    leecher_link = (
        f"ed2k://|file|{name}|{size}|{root}|"
        f"sources,{non_loopback_ipv4()}:{seed.ed2k_port}|/"
    )
    seed_gid = seed.add_uri(seed_link, {**options, "dir": str(seed.download_dir)})
    seed.rpc.wait_content_complete(seed_gid, 45)
    seed_status = wait_sharing(seed, seed_gid, 15)

    started = time.monotonic()
    leecher_gid = leecher.add_uri(leecher_link, {**options, "dir": str(leecher.download_dir)})
    leecher_status, peers = wait_download(leecher, leecher_gid, timeout)
    duration = round(time.monotonic() - started, 3)
    if sha256(leecher.download_dir / name) != expected:
        raise RuntimeError(f"ED2K payload digest mismatch for {name}")
    seed_ed2k = seed_status.get("ed2k", {})
    leecher_ed2k = leecher_status.get("ed2k", {})
    if not isinstance(seed_ed2k, dict) or not isinstance(leecher_ed2k, dict):
        raise RuntimeError("ED2K RPC state is missing")
    if seed_ed2k.get("kadRouterCount") != "0" or leecher_ed2k.get("kadRouterCount") != "0":
        raise RuntimeError("Inline ED2K sources leaked into the Kad routing table")
    # The inline source is the only one: the seed must have sent it all.
    uploaded = int(str(seed.rpc.call("aria2.tellStatus", [seed_gid]).get("uploadLength", "0")))
    if uploaded < size:
        raise RuntimeError(
            f"The inline ED2K source uploaded {uploaded} bytes of {name}, not the whole file"
        )
    return {
        "sha256": expected,
        "ed2k": root,
        "bytes": size,
        "durationSeconds": duration,
        "seedStatus": seed_status.get("status"),
        "leecherStatus": leecher_status.get("status"),
        "peerCount": peers,
        "seedUploaded": uploaded,
    }


def validate(run: RunDirectory, engine_path: Path | None) -> dict[str, object]:
    seed = EngineProcess(run, "seed", engine_path)
    leecher = EngineProcess(run, "leecher", engine_path)
    single = create_payload(seed.download_dir / "payload.bin", 1024 * 1024)
    # Two full parts and a tail: hash set exchange and compressed blocks.
    multipart = create_text_payload(seed.download_dir / "parts.txt", 2 * PART_SIZE + 1234567)
    nodes = run.fixtures / "empty-nodes.dat"
    nodes.write_bytes(struct.pack("<III", 0, 2, 0))

    seed.start()
    leecher.start()
    try:
        options = {
            "auto-file-renaming": "false",
            "allow-overwrite": "true",
            "ed2k-node-list": str(nodes),
            "seed-time": "10",
        }
        return {
            "single": transfer(seed, leecher, "payload.bin", single, options, 15),
            "multipart": transfer(seed, leecher, "parts.txt", multipart, options, 60),
        }
    finally:
        leecher.stop()
        seed.stop()


if __name__ == "__main__":
    raise SystemExit(run_validation("ed2k", validate))
