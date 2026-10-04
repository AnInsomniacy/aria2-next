#!/usr/bin/env python3
"""Exercise a built executable through real HTTP writes and RPC."""

from __future__ import annotations

from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from core.engine import EngineProcess
from core.report import run_validation
from core.runtime import RunDirectory, create_payload, process_options, sha256


class FixtureHandler(SimpleHTTPRequestHandler):
    def log_message(self, format: str, *args: object) -> None:
        pass


def validate(run: RunDirectory, engine_path: Path | None) -> dict[str, object]:
    payload = run.fixtures / "payload.bin"
    expected = create_payload(payload, 1024 * 1024)
    handler = partial(FixtureHandler, directory=str(run.fixtures))
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    uri = f"http://127.0.0.1:{server.server_port}/payload.bin"
    engine = EngineProcess(run, "rpc", engine_path)
    try:
        cli_dir = run.downloads / "cli"
        cli_dir.mkdir()
        cli_state = run.state / "cli"
        command = [
            str(engine.engine), "--no-conf=true", "--enable-dht=false",
            "--bt-port-mapping=false", "--file-allocation=none",
            "--stream-max-connections=1", "--max-tries=1",
            f"--state-dir={cli_state}", f"--dir={cli_dir}", uri,
        ]
        with (run.logs / "cli.log").open("wb") as output:
            result = subprocess.run(
                command, stdout=output, stderr=subprocess.STDOUT,
                timeout=30, **process_options(),
            )
        if result.returncode:
            raise RuntimeError(f"CLI exited with code {result.returncode}")
        if sha256(cli_dir / payload.name) != expected:
            raise RuntimeError("CLI output checksum mismatch")

        with engine:
            version = engine.rpc.call("aria2.getVersion")
            gid = engine.add_uri(uri, {"stream-max-connections": "1"})
            engine.rpc.wait_complete(gid, timeout=30)
            if sha256(engine.download_dir / payload.name) != expected:
                raise RuntimeError("RPC output checksum mismatch")
            engine.rpc.call("aria2.getGlobalStat")
            engine.rpc.call("aria2.shutdown")
            if engine.process is None:
                raise RuntimeError("RPC process was not started")
            code = engine.process.wait(timeout=15)
            if code:
                raise RuntimeError(f"RPC engine exited with code {code}")
        return {"version": version["version"], "sha256": expected,
                "cli": "complete", "rpc": "complete", "shutdown": "clean"}
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    raise SystemExit(run_validation("http-smoke", validate))
