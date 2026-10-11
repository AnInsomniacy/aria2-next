"""HTTP download benchmark against the adversarial origin in server.py.

Each scenario downloads one synthetic file through JSON-RPC and reports wall
time, the 95% -> 100% tail time, engine CPU time, peak memory and payload
correctness. Results are printed as a table and written as JSON.

  python bench.py --engine build/default/aria2-next.exe
  python bench.py --engine NEW --compare results-old.json --scenario stall,reset
"""

from __future__ import annotations

import argparse
import json
import os
import secrets
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from server import verify  # noqa: E402

MIB = 1 << 20
RATE = 4 * MIB

# name -> (query, size, connections, timeout seconds)
SCENARIOS = {
    "clean": (f"rate={RATE}", 256 * MIB, 16, 120),
    "unlimited": ("", 1024 * MIB, 16, 120),
    "single": (f"rate={RATE}", 64 * MIB, 1, 120),
    "small": ("", 64 * 1024, 16, 30),
    "stall": (f"rate={RATE}&stallat={200 * MIB}", 256 * MIB, 16, 180),
    "reset": (f"rate={RATE}&resetat={128 * MIB}", 256 * MIB, 16, 180),
    "slowtail": (f"rate={RATE}&slowtail=0.05", 256 * MIB, 16, 180),
    "overload-503": (f"rate={RATE}&maxconn=4&code=503", 128 * MIB, 16, 180),
    "overload-429": (f"rate={RATE}&maxconn=4&code=429&retryafter=1", 128 * MIB, 16, 180),
    "badrange": (f"rate={RATE}&badrange=1", 128 * MIB, 16, 180),
    "norange": (f"rate={8 * RATE}&norange=1", 128 * MIB, 16, 180),
    "expiring": (f"rate={RATE}&expire=3", 128 * MIB, 16, 180),
    "iprate": (f"iprate={16 * MIB}", 256 * MIB, 16, 180),
    "fanout": (f"rate={2 * MIB}", 256 * MIB, 256, 180),
}


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class Rpc:
    def __init__(self, port: int, secret: str):
        self.url = f"http://127.0.0.1:{port}/jsonrpc"
        self.token = f"token:{secret}"

    def call(self, method: str, *params):
        body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": [self.token, *params]})
        request = urllib.request.Request(self.url, body.encode(), {"Content-Type": "application/json"})
        with urllib.request.urlopen(request, timeout=10) as response:
            reply = json.load(response)
        if "error" in reply:
            raise RuntimeError(f"{method}: {reply['error']}")
        return reply["result"]


def process_usage(pid: int) -> tuple[float, int]:
    """Returns (cpu seconds, peak working set bytes) of a live process."""
    if os.name != "nt":
        with open(f"/proc/{pid}/stat") as stat:
            fields = stat.read().rsplit(")", 1)[1].split()
        ticks = os.sysconf("SC_CLK_TCK")
        with open(f"/proc/{pid}/status") as status:
            peak = next(int(line.split()[1]) * 1024 for line in status if line.startswith("VmHWM"))
        return (int(fields[11]) + int(fields[12])) / ticks, peak
    import ctypes
    from ctypes import wintypes

    class Counters(ctypes.Structure):
        _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [
            (name, ctypes.c_size_t)
            for name in (
                "PeakWorkingSetSize",
                "WorkingSetSize",
                "QuotaPeakPagedPoolUsage",
                "QuotaPagedPoolUsage",
                "QuotaPeakNonPagedPoolUsage",
                "QuotaNonPagedPoolUsage",
                "PagefileUsage",
                "PeakPagefileUsage",
            )
        ]

    kernel = ctypes.windll.kernel32
    handle = kernel.OpenProcess(0x1000 | 0x0010, False, pid)
    try:
        times = [wintypes.FILETIME() for _ in range(4)]
        kernel.GetProcessTimes(handle, *[ctypes.byref(t) for t in times])
        cpu = sum((t.dwHighDateTime << 32 | t.dwLowDateTime) for t in times[2:]) / 1e7
        counters = Counters()
        counters.cb = ctypes.sizeof(Counters)
        ctypes.windll.psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb)
        return cpu, counters.PeakWorkingSetSize
    finally:
        kernel.CloseHandle(handle)


def run(engine: Path, base: str, name: str, extra: list[str], keep: bool = False) -> dict:
    query, size, connections, timeout = SCENARIOS[name]
    work = Path(tempfile.mkdtemp(prefix=f"bench-{name}-"))
    port, secret = free_port(), secrets.token_hex(8)
    command = [
        str(engine),
        "--no-conf=true",
        "--enable-rpc=true",
        f"--rpc-listen-port={port}",
        f"--rpc-secret={secret}",
        f"--dir={work / 'out'}",
        f"--state-dir={work / 'state'}",
        "--summary-interval=0",
        "--show-console-readout=false",
        "--console-log-level=error",
        f"--log={work / 'engine.log'}",
        "--log-level=info",
        *extra,
    ]
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rpc = Rpc(port, secret)
    result = {"scenario": name, "size": size}
    try:
        for _ in range(100):
            try:
                rpc.call("aria2.getVersion")
                break
            except OSError:
                time.sleep(0.1)
        prefix = "signed/" if "expire" in query else ""
        url = f"{base}/{prefix}{secrets.token_hex(4)}/bench.bin?size={size}&{query}"
        options = {"split": str(connections), "max-connection-per-server": str(connections)}
        started = time.monotonic()
        gid = rpc.call("aria2.addUri", [url], options)
        tail_start = None
        status = {}
        while time.monotonic() - started < timeout:
            status = rpc.call("aria2.tellStatus", gid, ["status", "completedLength", "files", "errorMessage"])
            done = int(status["completedLength"])
            if tail_start is None and done >= size * 0.95:
                tail_start = time.monotonic()
            if status["status"] in ("complete", "error", "removed"):
                break
            time.sleep(0.05)
        elapsed = time.monotonic() - started
        cpu, peak = process_usage(process.pid)
        result.update(
            status=status.get("status", "timeout") if elapsed < timeout else "timeout",
            seconds=round(elapsed, 2),
            tail=round(time.monotonic() - tail_start, 2) if tail_start else None,
            mibps=round(size / MIB / elapsed, 1),
            cpu=round(cpu, 2),
            peak_mib=round(peak / MIB, 1),
        )
        if result["status"] == "complete":
            path = status["files"][0]["path"]
            bad = verify(path, size)
            result["correct"] = bad is None
            if bad is not None:
                result["first_bad_offset"] = bad
        else:
            result["error"] = status.get("errorMessage", "")
    finally:
        process.kill()
        process.wait()
        if keep:
            result["work"] = str(work)
        else:
            shutil.rmtree(work, ignore_errors=True)
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--scenario", default=",".join(SCENARIOS))
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--compare", type=Path)
    parser.add_argument("--keep", action="store_true", help="keep work directories and engine logs")
    parser.add_argument("extra", nargs="*", help="extra engine options")
    args = parser.parse_args()

    port = args.port or free_port()
    server = subprocess.Popen(
        [sys.executable, "-I", str(Path(__file__).parent / "server.py"), "--hosts", "127.0.0.1", "--port", str(port)],
        stdout=subprocess.PIPE,
    )
    server.stdout.readline()
    baseline = {}
    if args.compare:
        baseline = {item["scenario"]: item for item in json.loads(args.compare.read_text())}
    results = []
    try:
        for name in args.scenario.split(","):
            result = run(args.engine, f"http://127.0.0.1:{port}", name, args.extra, args.keep)
            results.append(result)
            before = baseline.get(name)
            versus = f"  (was {before['seconds']}s tail {before['tail']}s)" if before else ""
            print(
                f"{name:14} {result['status']:8} {result.get('seconds', '-'):>7}s "
                f"tail {result.get('tail')}s {result.get('mibps', '-')} MiB/s "
                f"cpu {result.get('cpu')}s peak {result.get('peak_mib')} MiB "
                f"ok={result.get('correct')} {result.get('error', '')}{versus} {result.get('work', '')}",
                flush=True,
            )
    finally:
        server.kill()
    if args.out:
        args.out.write_text(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
