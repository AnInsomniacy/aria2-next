#!/usr/bin/env python3
"""Offline, slow regression for cross-tier UDP tracker prioritization (#79)."""

from __future__ import annotations

import hashlib
import http.server
import json
import socketserver
import struct
import sys
import threading
import time
from contextlib import ExitStack
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from core.engine import EngineProcess
from core.report import run_validation
from core.rpc import RpcError
from core.runtime import RunDirectory

OBSERVE_SECONDS = 90


def bencode(value) -> bytes:
    if isinstance(value, int):
        return b"i" + str(value).encode() + b"e"
    if isinstance(value, str):
        value = value.encode()
    if isinstance(value, bytes):
        return str(len(value)).encode() + b":" + value
    if isinstance(value, list):
        return b"l" + b"".join(map(bencode, value)) + b"e"
    return b"d" + b"".join(bencode(key) + bencode(value[key]) for key in sorted(value)) + b"e"


class TrackerFixture:
    """Finite failures on every endpoint, followed by a long-lived success on A."""

    def __init__(self):
        self.lock = threading.Lock()
        self.records: list[dict] = []

    def record(self, role: str) -> bool:
        with self.lock:
            seen = {row["role"] for row in self.records}
            success = role == "A" and {"A", "H", "B", "U"} <= seen
            self.records.append({"time": time.monotonic(), "role": role, "success": success})
            return success

    def snapshot(self) -> list[dict]:
        with self.lock:
            return list(self.records)

    def http(self, host: str, role: str):
        fixture = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                success = fixture.record(role)
                payload = ({"interval": 3600, "min interval": 300, "peers": b"",
                            "complete": 0, "incomplete": 1} if success else
                           {"failure reason": "local tracker fault"})
                body = bencode(payload)
                # HTTP errors take the finite failure backoff, not the 300s
                # successful-response minimum announce interval.
                self.send_response(200 if success else 503)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *_):
                pass

        class Server(http.server.ThreadingHTTPServer):
            daemon_threads = True

            def server_bind(self):
                # Avoid HTTPServer's reverse DNS lookup for numeric loopback.
                socketserver.TCPServer.server_bind(self)
                self.server_name, self.server_port = self.server_address

        return Server((host, 0), Handler)

    def udp(self):
        fixture = self

        class Handler(socketserver.BaseRequestHandler):
            def handle(self):
                data, sock = self.request
                if len(data) < 16:
                    return
                _, transaction = struct.unpack("!II", data[8:16])
                fixture.record("U")
                sock.sendto(struct.pack("!II", 3, transaction) + b"local tracker fault",
                            self.client_address)

        return socketserver.UDPServer(("127.0.0.2", 0), Handler)


def start_server(stack: ExitStack, server) -> None:
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    def close():
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

    stack.callback(close)


def check_trackers(trackers: list[dict], urls: list[str]) -> None:
    by_url = {row["url"]: row for row in trackers}
    if set(by_url) != set(urls) or len(trackers) != 4:
        raise AssertionError(f"Unexpected tracker set: {trackers}")
    tiers = [int(row["tier"]) for row in trackers]
    if tiers != sorted(tiers):
        raise AssertionError(f"UDP prioritization broke tier ordering: {trackers}")
    # The same-host UDP replaces H in tier 0; H becomes the tier 2 backup.
    expected = dict(zip(urls, (0, 2, 1, 0)))
    if {url: int(row["tier"]) for url, row in by_url.items()} != expected:
        raise AssertionError(f"Cross-tier UDP swap lost position tiers: {trackers}")


def observe(engine: EngineProcess, gid: str, urls: list[str], fixture: TrackerFixture,
            samples: list[dict]) -> dict:
    started = time.monotonic()
    attached_deadline = started + 10
    working_at = None
    while time.monotonic() - started < OBSERVE_SECONDS:
        try:
            trackers = engine.rpc.call("aria2.getBtTrackers", [gid])
        except RpcError:
            if not samples and time.monotonic() < attached_deadline:
                time.sleep(0.1)
                continue
            raise
        if not trackers and not samples and time.monotonic() < attached_deadline:
            time.sleep(0.1)
            continue
        samples.append({"elapsed": round(time.monotonic() - started, 3), "trackers": trackers})
        check_trackers(trackers, urls)
        a = next(row for row in trackers if row["url"] == urls[0])
        if a["verified"] == "true" and int(a["nextAnnounce"]) > 0:
            if working_at is None:
                working_at = time.monotonic()
        elif working_at is not None:
            raise AssertionError(f"Working primary lost its future deadline: {a}")
        status = engine.rpc.call("aria2.tellStatus", [gid])
        if status["status"] != "active":
            raise AssertionError(f"Tracker-only task stopped: {status}")
        time.sleep(1)

    records = fixture.snapshot()
    counts = {role: sum(row["role"] == role for row in records) for role in "AHBU"}
    if working_at is None or counts["A"] < 2 or counts["B"] < 2 or counts["U"] < 1:
        raise AssertionError(f"Failure/recovery preconditions not reached: {counts}")
    # H used to remain at failures=2 and nextAnnounce=0 after ~72 seconds.
    # Require a real network retry after A succeeds, past H's second backoff.
    h_retries = [row for row in records if row["role"] == "H"]
    if len(h_retries) < 3 or h_retries[-1]["time"] <= working_at:
        raise AssertionError(f"Expired backup made no retry progress: {h_retries}")
    return {"rpcSamples": len(samples), "requests": counts,
            "observationSeconds": round(time.monotonic() - started, 3)}


def validate(run: RunDirectory, engine_path: Path | None) -> dict:
    fixture = TrackerFixture()
    samples: list[dict] = []
    with ExitStack() as stack:
        servers = []
        factories = [lambda: fixture.http("127.0.0.1", "A"),
                     lambda: fixture.http("127.0.0.2", "H"),
                     lambda: fixture.http("127.0.0.3", "B"), fixture.udp]
        for factory in factories:
            server = factory()
            start_server(stack, server)
            servers.append(server)
        urls = [f"{scheme}://{server.server_address[0]}:{server.server_address[1]}/announce/{role}"
                for server, scheme, role in zip(servers, ("http", "http", "http", "udp"), "AHBU")]
        payload = b"missing local content"
        torrent = run.fixtures / "tracker.torrent"
        torrent.write_bytes(bencode({"announce": urls[0],
                                    "announce-list": [[urls[0], urls[1]], [urls[2]], [urls[3]]],
                                    "info": {"length": len(payload), "name": "missing.bin",
                                             "piece length": 16384, "private": 1,
                                             "pieces": hashlib.sha1(payload).digest()}}))
        engine = EngineProcess(run, "tracker", engine_path)
        try:
            engine.start(["--bt-interface=127.0.0.1", "--disable-ipv6=true",
                          "--bt-dht-bootstrap-nodes="])
            gid = engine.add_torrent(torrent)
            return observe(engine, gid, urls, fixture, samples)
        finally:
            try:
                engine.stop()
            finally:
                (run.logs / "tracker-requests.json").write_text(
                    json.dumps(fixture.snapshot(), indent=2) + "\n", encoding="utf-8")
                (run.logs / "tracker-samples.json").write_text(
                    json.dumps(samples, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(run_validation("bittorrent_tracker", validate))
