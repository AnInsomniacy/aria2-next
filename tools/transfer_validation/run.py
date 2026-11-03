#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BUILD_ROOT = ROOT.parents[1] / "build" / "transfer-validation"
PROTOCOLS = ("http", "sftp", "bittorrent", "ed2k", "metalink", "media")
SLOW_MODULES = {"bittorrent_tracker": "bittorrent/tracker.py"}


def run_protocol(protocol: str, forwarded: list[str]) -> dict[str, object]:
    script = ROOT / SLOW_MODULES[protocol] if protocol in SLOW_MODULES else ROOT / protocol / "validate.py"
    command = [sys.executable, str(script), *forwarded]
    completed = subprocess.run(command, check=False)
    result: dict[str, object] = {
        "protocol": protocol,
        "exitCode": completed.returncode,
    }
    candidates = sorted(
        (BUILD_ROOT / "runs").glob(f"*Z-{protocol}/result.json"),
        key=lambda path: path.stat().st_mtime,
    )
    if candidates:
        result["report"] = json.loads(candidates[-1].read_text(encoding="utf-8"))
    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run Aria2 Next transfer validation modules."
    )
    parser.add_argument("protocol", choices=(*PROTOCOLS, *SLOW_MODULES, "all"))
    parser.add_argument("--engine", type=Path)
    parser.add_argument("--keep-artifacts", action="store_true")
    args = parser.parse_args()

    forwarded: list[str] = []
    if args.engine:
        forwarded.extend(("--engine", str(args.engine)))
    if args.keep_artifacts:
        forwarded.append("--keep-artifacts")

    selected = PROTOCOLS if args.protocol == "all" else (args.protocol,)
    results = [run_protocol(protocol, forwarded) for protocol in selected]
    generated = datetime.now(timezone.utc)
    summary = {
        "generatedAt": generated.isoformat(),
        "results": results,
        "success": all(result["exitCode"] == 0 for result in results),
    }
    reports = BUILD_ROOT / "reports"
    reports.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(summary, indent=2) + "\n"
    report = reports / f"{generated.strftime('%Y%m%dT%H%M%SZ')}-summary.json"
    report.write_text(serialized, encoding="utf-8")
    (reports / "latest.json").write_text(serialized, encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if summary["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
