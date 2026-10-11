from __future__ import annotations

import base64
import secrets
import os
import subprocess
from pathlib import Path
from typing import Any

from .rpc import RpcClient, RpcError
from .runtime import REPOSITORY_ROOT, RunDirectory, free_port, process_options


class EngineProcess:
    def __init__(
        self,
        run: RunDirectory,
        name: str,
        engine: Path | None = None,
        listen_port: int | None = None,
        ed2k_port: int | None = None,
    ):
        self.run = run
        self.name = name
        default_name = "aria2-next.exe" if os.name == "nt" else "aria2-next"
        self.engine = (engine or REPOSITORY_ROOT / "build/default" / default_name).resolve()
        if not self.engine.is_file():
            raise FileNotFoundError(f"Engine binary does not exist: {self.engine}")
        self.rpc_port = free_port()
        self.listen_port = listen_port or free_port()
        self.ed2k_port = ed2k_port or free_port()
        self.ed2k_udp_port = free_port()
        self.secret = secrets.token_hex(16)
        self.root = run.state / name
        self.download_dir = run.downloads / name
        self.root.mkdir(parents=True, exist_ok=True)
        self.download_dir.mkdir(parents=True, exist_ok=True)
        self.stdout_path = run.logs / f"{name}.stdout.log"
        self.stderr_path = run.logs / f"{name}.stderr.log"
        self.engine_log = run.logs / f"{name}.engine.log"
        self.process: subprocess.Popen[bytes] | None = None
        self.stdout = None
        self.stderr = None
        self.rpc = RpcClient(self.rpc_port, self.secret)

    def start(self, extra_options: list[str] | None = None) -> None:
        command = [
            str(self.engine),
            "--no-conf=true",
            "--enable-rpc=true",
            "--rpc-listen-all=false",
            f"--rpc-listen-port={self.rpc_port}",
            f"--rpc-secret={self.secret}",
            f"--state-dir={self.root}",
            f"--dir={self.download_dir}",
            f"--listen-port={self.listen_port}",
            f"--ed2k-listen-port={self.ed2k_port}",
            f"--ed2k-udp-listen-port={self.ed2k_udp_port}",
            "--bt-port-mapping=false",
            "--ed2k-port-mapping=false",
            "--enable-dht=false",
            "--bt-enable-lpd=false",
            "--enable-peer-exchange=false",
            "--file-allocation=none",
            "--summary-interval=0",
            "--show-console-readout=false",
            "--console-log-level=warn",
            f"--log={self.engine_log}",
            "--log-level=debug",
        ]
        if extra_options:
            command.extend(extra_options)
        self.stdout = self.stdout_path.open("ab")
        self.stderr = self.stderr_path.open("ab")
        self.process = subprocess.Popen(
            command, stdout=self.stdout, stderr=self.stderr, **process_options()
        )
        try:
            self.rpc.wait_ready()
        except Exception:
            self.stop()
            raise

    def add_uri(self, uri: str, options: dict[str, str] | None = None) -> str:
        return str(self.rpc.call("aria2.addUri", [[uri], options or {}]))

    def add_torrent(
        self, torrent: Path, options: dict[str, str] | None = None
    ) -> str:
        encoded = base64.b64encode(torrent.read_bytes()).decode()
        return str(self.rpc.call("aria2.addTorrent", [encoded, [], options or {}]))

    def add_metalink(
        self, metalink: Path, options: dict[str, str] | None = None
    ) -> list[str]:
        encoded = base64.b64encode(metalink.read_bytes()).decode()
        result = self.rpc.call("aria2.addMetalink", [encoded, options or {}])
        return [str(gid) for gid in result]

    def stop(self) -> None:
        if self.process is None:
            return
        if self.process.poll() is None:
            try:
                self.rpc.call("aria2.forceShutdown")
            except RpcError:
                pass
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=3)
        if self.stdout:
            self.stdout.close()
        if self.stderr:
            self.stderr.close()
        self.process = None

    def kill(self) -> None:
        """Ends the engine abruptly, as a crash or power loss would."""
        if self.process is None:
            return
        self.process.kill()
        self.process.wait(timeout=10)
        if self.stdout:
            self.stdout.close()
        if self.stderr:
            self.stderr.close()
        self.process = None

    def __enter__(self) -> "EngineProcess":
        self.start()
        return self

    def __exit__(self, *_: Any) -> None:
        self.stop()
