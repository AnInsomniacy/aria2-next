"""Network faults for bench.py through Toxiproxy (github.com/Shopify/toxiproxy).

Toxiproxy sits between the engine and server.py and applies "toxics" to the
downstream bytes: bandwidth, latency with jitter, a stall that holds the data
(timeout 0) or resets. A toxic's toxicity is the share of connections it
applies to. A scenario's toxics are a schedule of (second, change) steps
counted from the moment the download is added; static toxics start at 0.

Toxiproxy is found through --toxiproxy, TOXIPROXY_SERVER or PATH.
"""

from __future__ import annotations

import json
import os
import random
import shutil
import socket
import subprocess
import threading
import time
import urllib.request
from pathlib import Path


def toxic(name: str, kind: str, toxicity: float = 1.0, **attributes) -> dict:
    return {"name": name, "type": kind, "stream": "downstream", "toxicity": toxicity, "attributes": attributes}


def add(at: float, spec: dict) -> tuple[float, str, dict]:
    return (at, "add", spec)


def remove(at: float, name: str) -> tuple[float, str, dict]:
    return (at, "remove", {"name": name})


def periodic(spec: dict, period: float, on: float, until: float = 180) -> list:
    """The toxic is active during the last `on` seconds of every period."""
    steps, start = [], period - on
    while start < until:
        steps += [add(start, spec), remove(start + on, spec["name"])]
        start += period
    return steps


def walk(kbps: int, seed: str, until: float = 180) -> list:
    """Per-connection bandwidth that wanders between 5% and 100% of kbps."""
    rng, value, steps = random.Random(seed), 1.0, []
    for second in range(int(until)):
        value = min(1.0, max(0.05, value * 2 ** rng.uniform(-1, 1)))
        rate = max(1, int(kbps * value))
        steps.append((second, "add" if second == 0 else "update", toxic("walk", "bandwidth", rate=rate)))
    return steps


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class Toxiproxy:
    """A private toxiproxy-server with one proxy in front of the origin."""

    def __init__(self, upstream: str, binary: str | None = None):
        binary = binary or os.environ.get("TOXIPROXY_SERVER") or shutil.which("toxiproxy-server")
        if not binary or not Path(binary).exists():
            raise SystemExit("toxiproxy-server not found: pass --toxiproxy or set TOXIPROXY_SERVER")
        self.api_port, self.listen_port = free_port(), free_port()
        self.process = subprocess.Popen(
            [binary, "-host", "127.0.0.1", "-port", str(self.api_port)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        for _ in range(100):
            try:
                self._call("GET", "/version")
                break
            except OSError:
                time.sleep(0.05)
        self._call("POST", "/proxies", {"name": "origin", "listen": f"127.0.0.1:{self.listen_port}",
                                        "upstream": upstream, "enabled": True})
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()

    def _call(self, method: str, path: str, body: dict | None = None):
        data = json.dumps(body).encode() if body is not None else None
        request = urllib.request.Request(f"http://127.0.0.1:{self.api_port}{path}", data, method=method,
                                         headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(request, timeout=10) as response:
            payload = response.read()
        return json.loads(payload) if payload else None

    def _apply(self, action: str, spec: dict) -> None:
        if action == "add":
            self._call("POST", "/proxies/origin/toxics", spec)
        elif action == "update":
            self._call("POST", f"/proxies/origin/toxics/{spec['name']}",
                       {"attributes": spec["attributes"], "toxicity": spec["toxicity"]})
        else:
            self._call("DELETE", f"/proxies/origin/toxics/{spec['name']}")

    def start(self, schedule: list) -> None:
        """Clears earlier toxics, applies the steps at 0 s and runs the rest."""
        self.stop()
        self._call("POST", "/reset")
        steps = sorted(schedule, key=lambda step: step[0])
        while steps and steps[0][0] <= 0:
            self._apply(*steps.pop(0)[1:])
        if not steps:
            return
        self._stop.clear()
        begun = time.monotonic()

        def play():
            for at, action, spec in steps:
                if self._stop.wait(max(0.0, begun + at - time.monotonic())):
                    return
                self._apply(action, spec)

        self._thread = threading.Thread(target=play, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        if self._thread:
            self._stop.set()
            self._thread.join()
            self._thread = None

    def close(self) -> None:
        self.stop()
        self.process.kill()
        self.process.wait()
