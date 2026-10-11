#!/usr/bin/env python3

from __future__ import annotations

import base64
import hashlib
import json
import os
import re
import shutil
import sys
import time
import urllib.request
from urllib.parse import quote
from pathlib import Path

SUITE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SUITE_ROOT))

from core.engine import EngineProcess
from core.report import run_validation
from core.runtime import RunDirectory, create_payload, free_port, sha256
from core.services import CaddyService, ToxiproxyService, WireMockService, post_json


def validate(run: RunDirectory, engine_path: Path | None) -> dict[str, object]:
    payload = run.fixtures / "payload.bin"
    expected = create_payload(payload, 16 * 1024 * 1024)
    # A date validator must predate the response, not describe fixture creation.
    os.utime(payload, (time.time() - 120, time.time() - 120))
    automatic_name = "resume-é-下载%20.bin"
    shutil.copyfile(payload, run.fixtures / automatic_name)
    with payload.open("rb") as source:
        block = source.read(1024 * 1024)
        source.seek(4 * 1024 * 1024)
        response_body = source.read(1024 * 1024)
    (run.fixtures / "empty.bin").touch()
    (run.fixtures / "tiny.bin").write_bytes(block[:60651])
    large = run.fixtures / "large.bin"
    large_hash = hashlib.sha256()
    with large.open("wb") as output:
        for index in range(320):
            chunk = hashlib.sha256(index.to_bytes(8, "big")).digest() + block[32:]
            output.write(chunk)
            large_hash.update(chunk)

    results: dict[str, object] = {}
    with CaddyService(run, "http", run.fixtures) as caddy, WireMockService(
        run, "http"
    ) as wiremock, ToxiproxyService(run, "http") as proxy:
        wiremock.stub({
            "priority": 10,
            "request": {"method": "GET", "urlPath": "/payload.bin"},
            "response": {"proxyBaseUrl": caddy.base_url},
        })
        begin, end = 4 * 1024 * 1024, 5 * 1024 * 1024
        requested = f"bytes={begin}-{end - 1}"
        for case in ("short", "tail", "tail-retry"):
            body = response_body if case == "tail" else response_body[:-65536]
            response = {
                "status": 206,
                "base64Body": base64.b64encode(body).decode(),
                "headers": {
                    "Content-Range":
                        f"bytes {begin}-{begin + len(body) - 1}/{payload.stat().st_size}"
                },
            }
            if case == "tail":
                response["chunkedDribbleDelay"] = {
                    "numberOfChunks": 16, "totalDuration": 8000,
                }
            wiremock.stub({
                "priority": 1,
                "request": {
                    "method": "GET", "url": f"/payload.bin?case={case}",
                    "headers": {"Range": {"equalTo": requested}},
                },
                "response": response,
            })
        # Replacement requests also pay response latency. Changing the Range
        # must not turn every slow-tail test into an instant healthy response.
        wiremock.stub({
            "priority": 5,
            "request": {"method": "GET", "url": "/payload.bin?case=tail"},
            "response": {"proxyBaseUrl": caddy.base_url,
                         "fixedDelayMilliseconds": 1000},
        })
        wiremock.stub({
            "priority": 1,
            "scenarioName": "tail-retry",
            "requiredScenarioState": "Started",
            "newScenarioState": "Recovered",
            "request": {
                "method": "GET", "url": "/payload.bin?case=tail-retry",
                "headers": {"Range": {"matches": rf"bytes=(?!{begin}-)[0-9]+-{end - 1}"}},
            },
            "response": {"status": 503, "headers": {"Retry-After": "1"}},
        })
        wiremock.stub({
            "priority": 1,
            "request": {
                "method": "GET", "url": "/payload.bin?case=delayed",
                "headers": {"Range": {"matches": "bytes=[1-9][0-9]*-.*"}},
            },
            "response": {
                "proxyBaseUrl": caddy.base_url, "fixedDelayMilliseconds": 3000,
            },
        })
        for code in (429, 503):
            wiremock.stub({
                "priority": 1,
                "scenarioName": f"retry-{code}",
                "requiredScenarioState": "Started",
                "newScenarioState": "Recovered",
                "request": {
                    "method": "GET", "url": f"/payload.bin?case=retry-{code}",
                    "headers": {"Range": {"equalTo": requested}},
                },
                "response": {
                    "status": code, "headers": {"Retry-After": "1"},
                },
            })

        with urllib.request.urlopen(caddy.base_url + "/payload.bin") as response:
            etag = response.headers["ETag"]
            modified = response.headers["Last-Modified"]
        wiremock.file("payload.bin", payload)
        whole = {"status": 200, "bodyFileName": "payload.bin",
                 "headers": {"ETag": etag, "Last-Modified": modified}}
        for header in ("If-Match", "If-Unmodified-Since"):
            wiremock.stub({
                "priority": 1,
                "request": {"method": "GET", "url": "/download?token=fixture",
                            "headers": {header: {"matches": ".+"}}},
                "response": {"status": 412},
            })
        wiremock.stub({
            "priority": 10,
            "request": {"method": "GET", "url": "/download?token=fixture"},
            "response": {"status": 302, "headers": {
                "Location": "/payload.bin?case=redirect",
                "Set-Cookie": "fixture-session=valid; Path=/; HttpOnly",
                "ETag": '"redirect-not-file"',
            }},
        })
        wiremock.stub({
            "priority": 1,
            "request": {"method": "GET", "url": "/payload.bin?case=redirect",
                        "cookies": {"fixture-session": {"equalTo": "valid"}}},
            "response": {"proxyBaseUrl": caddy.base_url},
        })
        wiremock.stub({
            "priority": 2,
            "request": {"method": "GET", "url": "/payload.bin?case=redirect"},
            "response": {"status": 401},
        })
        wiremock.stub({
            "priority": 1,
            "request": {"method": "GET", "url": "/payload.bin?case=date"},
            "response": {"proxyBaseUrl": caddy.base_url,
                         "headers": {"ETag": 'W/"weak"'}},
        })
        for nonzero in (False, True):
            # Dynamic endpoints sometimes stamp each response with its current
            # time. Such dates must not become strong range validators.
            fresh_date = f"Tue, 08 Sep 2026 23:00:0{int(nonzero)} GMT"
            wiremock.stub({
                "priority": 1,
                "request": {
                    "method": "GET", "url": "/payload.bin?case=fresh-date",
                    "headers": {"Range": {"matches":
                        "bytes=[1-9][0-9]*-.*" if nonzero else "bytes=0-.*"}},
                },
                "response": {"proxyBaseUrl": caddy.base_url, "headers": {
                    "ETag": 'W/"weak"', "Date": fresh_date,
                    "Last-Modified": fresh_date,
                }},
            })
            wiremock.stub({
                "priority": 1,
                "request": {
                    "method": "GET", "url": "/payload.bin?case=bare-etag",
                    "headers": {"Range": {"matches":
                        "bytes=[1-9][0-9]*-.*" if nonzero else "bytes=0-.*"}},
                },
                "response": {
                    "proxyBaseUrl": caddy.base_url,
                    "headers": {
                        "ETag": etag.strip('"'),
                        "Last-Modified": ("Fri, 24 Apr 2026 06:48:00 GMT" if nonzero
                                          else modified),
                    },
                },
            })
        for case in ("ignored", "changed-200", "changed-206", "changed-412", "416"):
            response = whole
            if case == "changed-200":
                response = {**whole, "headers": {"ETag": '"changed"'}}
            elif case == "changed-206":
                response = {"proxyBaseUrl": caddy.base_url,
                            "headers": {"ETag": '"changed"'}}
            elif case in ("changed-412", "416"):
                response = {"status": 412 if case == "changed-412" else 416}
            wiremock.stub({
                "priority": 1,
                "request": {"method": "GET", "url": f"/payload.bin?case={case}",
                            "headers": {"Range": {"matches": "bytes=[1-9][0-9]*-.*"}}},
                "response": response,
            })

        # A signed entry point is not a range server. Reuse its validated
        # destination, and serialize refresh when that destination expires.
        for expired_code in (401, 403, 404):
            entry = f"/entry?refresh={expired_code}"
            old = f"/payload.bin?case=endpoint-old-{expired_code}"
            new = f"/payload.bin?case=endpoint-new-{expired_code}"
            for state, next_state, response in (
                ("Started", "issued", {"status": 302, "headers": {"Location": old}}),
                ("issued", "retry", {"status": 503, "headers": {"Retry-After": "1"}}),
                ("retry", "renewed", {"status": 302, "headers": {"Location": new}}),
            ):
                wiremock.stub({
                    "scenarioName": f"endpoint-refresh-{expired_code}",
                    "requiredScenarioState": state, "newScenarioState": next_state,
                    "request": {"method": "GET", "url": entry},
                    "response": response,
                })
            wiremock.stub({
                "priority": 1,
                "request": {"method": "GET", "url": old,
                            "headers": {"Range": {"matches": "bytes=[1-9][0-9]*-.*"}}},
                "response": {"status": expired_code},
            })
            wiremock.stub({
                "priority": 20, "request": {"method": "GET", "url": entry},
                "response": {"status": 403},
            })
        wiremock.stub({
            "priority": 20, "request": {"method": "GET", "url": "/entry?once"},
            "response": {"status": 403},
        })
        wiremock.stub({
            "scenarioName": "endpoint-once", "requiredScenarioState": "Started",
            "newScenarioState": "issued",
            "request": {"method": "GET", "url": "/entry?once"},
            "response": {"status": 302, "headers": {
                "Location": "/payload.bin?case=endpoint-once"}},
        })
        wiremock.stub({
            "request": {"method": "GET", "url": "/entry?credentials",
                        "headers": {"Authorization": {"equalTo": "Bearer fixture"}}},
            "response": {"status": 302, "headers": {
                "Location": f"http://localhost:{wiremock.port}/payload.bin?case=credentials"}},
        })
        wiremock.stub({
            "priority": 1,
            "request": {"method": "GET", "url": "/payload.bin?case=credentials",
                        "headers": {"Authorization": {"absent": True},
                                    "Cookie": {"absent": True}}},
            "response": {"proxyBaseUrl": caddy.base_url},
        })
        wiremock.stub({
            "priority": 2,
            "request": {"method": "GET", "url": "/payload.bin?case=credentials"},
            "response": {"status": 403},
        })
        wiremock.stub({
            "request": {"method": "GET", "url": "/entry?family-bound"},
            "response": {"status": 302, "headers": {
                "Location": f"http://localhost:{wiremock.port}/payload.bin?case=family-bound"}},
        })
        wiremock.stub({
            "priority": 1,
            "request": {"method": "GET", "url": "/payload.bin?case=family-bound"},
            "response": {"status": 403},
        })

        wiremock.stub({
            "request": {"method": "GET", "url": "/named-download"},
            "response": {"status": 200, "base64Body": base64.b64encode(block).decode(),
                         "headers": {"Content-Disposition": "attachment; filename=fallback.bin; filename*=UTF-8''report-%E4%B8%AD%E6%96%87%2520.bin"}},
        })
        wiremock.stub({
            "request": {"method": "GET", "url": "/name-redirect"},
            "response": {"status": 302, "headers": {"Location": wiremock.base_url + "/named-download"}},
        })
        wiremock.stub({
            "priority": 1,
            "request": {"method": "GET", "url": "/payload.bin?case=naming-resume"},
            "response": {"proxyBaseUrl": caddy.base_url, "headers": {"Content-Disposition": "attachment; filename=resume%20.bin"}},
        })
        engine = EngineProcess(run, "engine", engine_path)
        session = run.state / "download.session"
        engine.start([f"--save-session={session}"])
        options = {
            "stream-max-connections": "64",
            "max-tries": "4",
            "retry-wait": "1",
            "auto-file-renaming": "false",
        }

        def check(
            name: str, url: str, overrides: dict[str, str] | None = None,
            digest: str = expected, restart: bool = False,
        ) -> str:
            started = time.monotonic()
            gid = engine.add_uri(
                url, {**options, "out": name, **(overrides or {})}
            )
            if restart:
                engine.rpc.wait_status(gid, "complete", 30)
            else:
                engine.rpc.wait_complete(gid, 30)
            if sha256(engine.download_dir / name) != digest:
                raise RuntimeError(f"Payload digest mismatch: {name}")
            results[name] = round(time.monotonic() - started, 3)
            return gid

        try:
            for label, overrides, expected_name in [
                ("response", {}, "report-中文%20.bin"),
                ("hint", {"filename-hint": "README"}, "report-中文%20.bin"),
                ("browser", {"filename-hint": "browser%20.bin", "filename-hint-source": "browser"}, "browser%20.bin"),
                ("explicit", {"out": "chosen%20.bin", "filename-hint": "ignored.bin"}, "chosen%20.bin"),
            ]:
                directory = engine.download_dir / ("naming-" + label)
                gid = engine.add_uri(wiremock.base_url + "/name-redirect", {
                    **options, "dir": str(directory), "stream-max-connections": "1", **overrides,
                })
                task = engine.rpc.wait_complete(gid, 30)
                output = directory / expected_name
                if Path(task["files"][0]["path"]) != output or sha256(output) != hashlib.sha256(block).hexdigest():
                    raise RuntimeError(f"Response filename or payload mismatch: {label}")
                if (directory / "name-redirect").exists():
                    raise RuntimeError("A provisional filename was written before response naming")
                results["filename-" + label] = "passed"
            resume_dir = engine.download_dir / "naming-resume"
            resume_dir.mkdir()
            shutil.copyfile(payload, resume_dir / "resume%20.bin")
            gid = engine.add_uri(wiremock.base_url + "/payload.bin?case=naming-resume", {
                **options, "dir": str(resume_dir), "continue": "true",
            })
            task = engine.rpc.wait_complete(gid, 30)
            if sha256(resume_dir / "resume%20.bin") != expected or Path(task["files"][0]["path"]).name != "resume%20.bin":
                raise RuntimeError("Response naming did not retain the existing download")
            results["filename-existing-output"] = "passed"
            original = f"{caddy.base_url}/replaced-before-start.bin"
            replacement = f"{caddy.base_url}/payload.bin"
            gid = engine.add_uri(original, {
                **options, "out": "changed-uri.bin", "pause": "true",
            })
            changed = engine.rpc.call("aria2.changeUri", [
                gid, 1, [original], [replacement],
            ])
            if changed != [1, 1]:
                raise RuntimeError(f"URI replacement count mismatch: {changed}")
            engine.rpc.call("aria2.unpause", [gid])
            engine.rpc.wait_complete(gid, 30)
            if sha256(engine.download_dir / "changed-uri.bin") != expected:
                raise RuntimeError("URI replacement did not reach the stream backend")
            results["changedUri"] = "replacement downloaded with matching SHA-256"
            check("endpoint-once.bin", f"{wiremock.base_url}/entry?once")
            for expired_code in (401, 403, 404):
                check(f"endpoint-refresh-{expired_code}.bin",
                      f"{wiremock.base_url}/entry?refresh={expired_code}")
            check("endpoint-credentials.bin", f"{wiremock.base_url}/entry?credentials",
                  {"header": "Authorization: Bearer fixture\nCookie: manual=private"})
            # TCP/HTTP setup succeeds immediately on IPv6, but its body is
            # slow. The IPv4 listener serves the same bytes at the same port.
            dual_port = free_port()
            post_json(f"{proxy.api_url}/proxies", {
                "name": "fast-ipv4", "listen": f"127.0.0.1:{dual_port}",
                "upstream": f"127.0.0.1:{caddy.port}", "enabled": True,
            })
            proxy.add_toxic("fast-ipv4", "latency", "latency", {"latency": 100})
            post_json(f"{proxy.api_url}/proxies", {
                "name": "slow-ipv6", "listen": f"[::1]:{dual_port}",
                "upstream": f"127.0.0.1:{caddy.port}", "enabled": True,
            })
            proxy.add_toxic("slow-ipv6", "bandwidth", "bandwidth", {"rate": 32})
            dual_gid = check("dual-stack.bin", f"http://localhost:{dual_port}/payload.bin")
            if results["dual-stack.bin"] > 8:
                raise RuntimeError("The download remained on the slow IPv6 body path")
            # Delayed IPv4 workers must remain usable while IPv6 makes progress.
            # Verify actual payload on both routes after the buffered log flushes.
            wiremock.stub({
                "priority": 1,
                "request": {"method": "GET", "url": "/payload.bin?case=late-family"},
                "response": {"proxyBaseUrl": caddy.base_url,
                             "fixedDelayMilliseconds": 2500},
            })
            delayed_port = free_port()
            post_json(f"{proxy.api_url}/proxies", {
                "name": "delayed-ipv4", "listen": f"127.0.0.1:{delayed_port}",
                "upstream": f"127.0.0.1:{wiremock.port}", "enabled": True,
            })
            post_json(f"{proxy.api_url}/proxies", {
                "name": "early-ipv6", "listen": f"[::1]:{delayed_port}",
                "upstream": f"127.0.0.1:{caddy.port}", "enabled": True,
            })
            proxy.add_toxic("early-ipv6", "bandwidth", "bandwidth", {"rate": 32})
            delayed_gid = check(
                "delayed-family.bin",
                f"http://localhost:{delayed_port}/payload.bin?case=late-family",
            )
            # A healthy alternate must remain available after an early loss.
            # Change the incumbent's bandwidth only after useful progress,
            # using Toxiproxy rather than a custom HTTP implementation.
            changing_port = free_port()
            for family, listen, rate in (("ipv4", "127.0.0.1", 1024),
                                         ("ipv6", "[::1]", 2048)):
                name = f"changing-{family}"
                post_json(f"{proxy.api_url}/proxies", {
                    "name": name, "listen": f"{listen}:{changing_port}",
                    "upstream": f"127.0.0.1:{caddy.port}", "enabled": True,
                })
                proxy.add_toxic(name, "bandwidth", "bandwidth", {"rate": rate})
            proxy.add_toxic("changing-ipv4", "latency", "latency", {"latency": 100})
            started = time.monotonic()
            changing_gid = engine.add_uri(f"http://localhost:{changing_port}/payload.bin",
                {**options, "out": "changing-family.bin", "stream-max-connections": "4"})
            while time.monotonic() - started < 10:
                progress = engine.rpc.call("aria2.tellStatus", [changing_gid])
                if int(progress["completedLength"]) >= 8 * 1024 * 1024:
                    break
                time.sleep(0.05)
            else:
                raise RuntimeError("The changing-path fixture did not reach its transition")
            post_json(f"{proxy.api_url}/proxies/changing-ipv6/toxics/bandwidth",
                      {"attributes": {"rate": 32}})
            engine.rpc.wait_complete(changing_gid, 20)
            if sha256(engine.download_dir / "changing-family.bin") != expected:
                raise RuntimeError("Path reassignment corrupted the payload")
            results["changingFamily"] = round(time.monotonic() - started, 3)
            # A destination can be authorized on one address family only.
            # Rejection of a discovery transfer must not expire its good peer.
            post_json(f"{proxy.api_url}/proxies", {
                "name": "family-bound", "listen": f"[::1]:{wiremock.port}",
                "upstream": f"127.0.0.1:{caddy.port}", "enabled": True,
            })
            proxy.add_toxic("family-bound", "bandwidth", "bandwidth", {"rate": 16384})
            with EngineProcess(run, "family-bound", engine_path) as bound:
                bound.rpc.call("aria2.changeGlobalOption", [{"log-level": "trace"}])
                started = time.monotonic()
                bound_gid = bound.add_uri(f"{wiremock.base_url}/entry?family-bound",
                    {**options, "out": "payload.bin", "retry-wait": "10"})
                bound.rpc.wait_complete(bound_gid, 8)
                if sha256(bound.download_dir / "payload.bin") != expected:
                    raise RuntimeError("Address-family-bound destination corrupted the payload")
                results["familyBoundEndpoint"] = round(time.monotonic() - started, 3)
            if not re.search(r"event=range_finished .*http=403 ",
                             bound.engine_log.read_text(encoding="utf-8")):
                raise RuntimeError("The unavailable address family was not exercised")
            for connections in (1, 64):
                check(f"redirect-{connections}.bin",
                      f"{wiremock.base_url}/download?token=fixture",
                      {"stream-max-connections": str(connections)})
            check("bare-etag.bin", f"{wiremock.base_url}/payload.bin?case=bare-etag")
            check("fresh-date.bin", f"{wiremock.base_url}/payload.bin?case=fresh-date")
            automatic_url = f"{caddy.base_url}/{quote(automatic_name)}"
            for connections in (1, 64):
                directory = engine.download_dir / f"names-{connections}"
                directory.mkdir()
                gid = engine.add_uri(automatic_url, {
                    **options, "dir": str(directory),
                    "stream-max-connections": str(connections),
                })
                status = engine.rpc.wait_complete(gid, 30)
                output = directory / automatic_name
                if (Path(status["files"][0]["path"]) != output
                        or sha256(output) != expected):
                    raise RuntimeError("Automatic output filename mismatch")
            results["automaticFilename"] = "passed"
            for name in ("empty.bin", "tiny.bin"):
                check(name, f"{caddy.base_url}/{name}",
                      digest=sha256(run.fixtures / name))
            check("single.bin", f"{caddy.base_url}/payload.bin",
                  {"stream-max-connections": "1"})
            check("tls.bin", f"{wiremock.https_base_url}/payload.bin",
                  {"check-certificate": "false"})
            for case in ("short", "delayed", "tail", "retry-429", "retry-503"):
                check(f"{case}.bin",
                      f"{wiremock.base_url}/payload.bin?case={case}")
            tail_gid = check(
                "tail-retry.bin", f"{wiremock.base_url}/payload.bin?case=tail-retry",
                {"retry-wait": "10", "stream-max-connections": "256"})
            if results["tail-retry.bin"] < 10:
                raise RuntimeError("Tail recovery ignored the configured retry wait")

            for connections in (1, 2, 64, 256):
                check(f"conditional-{connections}.bin",
                      f"{wiremock.base_url}/payload.bin?case=conditional",
                      {"stream-max-connections": str(connections)})
            check("date.bin", f"{wiremock.base_url}/payload.bin?case=date")
            check("ignored.bin", f"{wiremock.base_url}/payload.bin?case=ignored",
                  restart=True)
            for case in ("changed-200", "changed-206", "changed-412", "416", "ignored"):
                name = f"protected-{case}.bin"
                path = engine.download_dir / name
                prefix = payload.read_bytes()[:4 * 1024 * 1024]
                if case == "ignored":
                    path.write_bytes(prefix)
                gid = engine.add_uri(
                    f"{wiremock.base_url}/payload.bin?case={case}",
                    {**options, "out": name, "continue": "true"},
                )
                status = engine.rpc.wait_status(gid, "error", 10)
                saved = path.read_bytes()
                # Written blocks must come from the validated representation;
                # unwritten blocks of a pre-sized output remain zero.
                original = payload.read_bytes()
                block = 64 * 1024
                foreign = any(
                    saved[i:i + block] != original[i:i + block]
                    and saved[i:i + block].count(0) != len(saved[i:i + block])
                    for i in range(0, len(saved), block))
                if (status.get("errorCode") != "8" or not saved
                        or len(saved) > len(original) or foreign
                        or (case == "ignored" and saved != prefix)):
                    raise RuntimeError(f"Unsafe range failure: {case}: {status}")
            results["conditionalResponses"] = "passed"

            # Native throttling keeps the 256-way transfer observable without
            # relying on an external server or a multi-gigabyte download.
            limited_port = proxy.create_proxy("fanout", caddy.port)
            proxy.add_toxic("fanout", "bandwidth", "bandwidth", {"rate": 2048})
            check("large.bin", f"http://127.0.0.1:{limited_port}/large.bin",
                  {"stream-max-connections": "256"}, large_hash.hexdigest())

            cut_port = proxy.create_proxy("cut", caddy.port)
            proxy.add_toxic("cut", "first_64k", "limit_data", {"bytes": 65536})
            gid = engine.add_uri(
                f"http://127.0.0.1:{cut_port}/payload.bin",
                {**options, "out": "cut.bin"},
            )
            time.sleep(0.5)
            proxy.remove_toxic("cut", "first_64k")
            engine.rpc.wait_complete(gid, 30)
            if sha256(engine.download_dir / "cut.bin") != expected:
                raise RuntimeError("Interrupted transfer digest mismatch")

            # Keep only the request evidence needed to detect lost or repeated
            # work; do not duplicate WireMock response bodies in the report.
            requests = []
            for entry in post_json(f"{wiremock.base_url}/__admin/requests/find",
                                   {"method": "GET"})["requests"]:
                headers = {k.lower(): v for k, v in entry["headers"].items()}
                requests.append({"url": entry["url"], "range": headers.get("range"),
                                 "ifMatch": headers.get("if-match"),
                                 "ifRange": headers.get("if-range"),
                                 "ifUnmodifiedSince": headers.get("if-unmodified-since")})
            (run.logs / "http-requests.json").write_text(
                json.dumps(requests, indent=2)
            )

            def ranges(case):
                return [r["range"] for r in requests
                        if r["url"] == f"/payload.bin?case={case}"]

            entry_counts = {"/entry?once": 1, "/entry?credentials": 2,
                            "/entry?family-bound": 2}
            entry_counts.update({f"/entry?refresh={code}": 3 for code in (401, 403, 404)})
            for path, expected_count in entry_counts.items():
                if sum(r["url"] == path for r in requests) != expected_count:
                    raise RuntimeError(f"Repeated or missing endpoint resolution: {path}")

            for case, value in (("conditional", etag), ("bare-etag", etag),
                                ("date", modified), ("redirect", etag)):
                partial = [r for r in requests
                           if r["url"] == f"/payload.bin?case={case}"
                           and r["range"] and not r["range"].startswith("bytes=0-")]
                if not partial or any(r["ifRange"] != value or r["ifMatch"]
                                      or r["ifUnmodifiedSince"] for r in partial):
                    raise RuntimeError(f"Incorrect range precondition: {case}")
            if sum(r["url"] == "/payload.bin?case=ignored" and not r["range"]
                   for r in requests) != 1:
                raise RuntimeError("Range fallback did not issue exactly one complete GET")
            if any(r["ifRange"] or r["ifMatch"] or r["ifUnmodifiedSince"]
                   for r in requests if r["url"] == "/payload.bin?case=fresh-date"):
                raise RuntimeError("An unqualified date became a range validator")

            short = ranges("short")
            suffix_pattern = rf"bytes=(?!{begin}-)[0-9]+-{end - 1}"
            if short.count(requested) != 1 or not any(
                    re.fullmatch(suffix_pattern, value) for value in short if value):
                raise RuntimeError("Short response left its final suffix unrecovered")
            retried = ranges("tail-retry")
            if not any(re.fullmatch(suffix_pattern, value) and retried.count(value) == 2
                       for value in retried if value):
                raise RuntimeError("The failed suffix was not retried exactly once")
            for code in (429, 503):
                if ranges(f"retry-{code}").count(requested) != 2:
                    raise RuntimeError(f"HTTP {code} retry was not exercised exactly once")
            # The first request is open-ended (bytes=N-); it covers the tail.
            tail = [(int(first), int(last) if last else payload.stat().st_size - 1)
                    for first, _, last in (value.removeprefix("bytes=").partition("-")
                                           for value in ranges("tail") if value)]
            if not any(begin < first <= last < end for first, last in tail):
                raise RuntimeError("Slow tail work was not reassigned")
            if len([r for r in tail if begin <= r[0] < end]) > 2 * (end - begin) // 65536:
                raise RuntimeError("Tail requests exceeded the bounded split and reassignment budget")

            bare_gid = engine.add_uri(
                f"{wiremock.base_url}/payload.bin?case=bare-etag",
                {**options, "out": "bare-resume.bin", "max-download-limit": "1M"},
            )
            gid = engine.add_uri(
                automatic_url,
                {**options, "max-download-limit": "4M"},
            )
            time.sleep(0.2)
            engine.rpc.call("aria2.pause", [gid])
            paused = engine.rpc.wait_status(gid, "paused")
            if not 0 < int(paused["completedLength"]) < int(paused["totalLength"]):
                raise RuntimeError("Pause did not retain partial progress")
            engine.rpc.call("aria2.pause", [bare_gid])
            bare_paused = engine.rpc.wait_status(bare_gid, "paused")
            if not 0 < int(bare_paused["completedLength"]) < int(bare_paused["totalLength"]):
                raise RuntimeError("Bare ETag fixture did not reach partial progress")
            engine.rpc.call("aria2.saveSession")
            engine.stop()
            shutil.copyfile(engine.engine_log, run.logs / "before-restart.engine.log")
            engine.start([f"--save-session={session}", f"--input-file={session}"])
            bare_restored = engine.rpc.wait_status(bare_gid, "paused")
            if any(bare_restored[k] != bare_paused[k]
                   for k in ("totalLength", "completedLength")):
                raise RuntimeError("Bare ETag progress changed across restart")
            engine.rpc.call("aria2.changeOption", [bare_gid, {"max-download-limit": "0"}])
            engine.rpc.call("aria2.unpause", [bare_gid])
            engine.rpc.wait_complete(bare_gid, 30)
            if sha256(engine.download_dir / "bare-resume.bin") != expected:
                raise RuntimeError("Bare ETag restart digest mismatch")
            restored = engine.rpc.wait_status(gid, "paused")
            if (any(restored[k] != paused[k]
                    for k in ("totalLength", "completedLength"))
                    or Path(restored["files"][0]["path"]) != engine.download_dir / automatic_name
                    or restored["files"][0]["path"] != paused["files"][0]["path"]):
                raise RuntimeError("Paused progress or filename changed across restart")
            engine.rpc.call("aria2.unpause", [gid])
            engine.rpc.wait_complete(gid, 30)
            if sha256(engine.download_dir / automatic_name) != expected:
                raise RuntimeError("Resumed transfer digest mismatch")

            gids = [
                engine.add_uri(
                    f"{caddy.base_url}/payload.bin",
                    {**options, "out": f"remove-{i}.bin", "max-download-limit": "1M"},
                ) for i in range(3)
            ]
            time.sleep(0.2)
            engine.rpc.call("aria2.forcePauseAll")
            for gid in gids:
                engine.rpc.wait_status(gid, "paused")
                engine.rpc.call("aria2.forceRemove", [gid])
            engine.rpc.call("aria2.purgeDownloadResult")
            if engine.rpc.call("aria2.tellActive") or engine.rpc.call(
                "aria2.tellWaiting", [0, 10]
            ):
                raise RuntimeError("Removed transfers remain in the engine")
            check("after-remove.bin", f"{caddy.base_url}/payload.bin")
            results["restartAndRemoval"] = "passed"
        finally:
            engine.stop()

        # The logger buffers debug output. Check the flushed pre-restart log,
        # rather than racing the logger immediately after RPC completion.
        retry_log = (run.logs / "before-restart.engine.log").read_text(encoding="utf-8")
        if not re.search(rf"event=route_payload gid={dual_gid} "
                         r"family=ipv6 bytes=[1-9][0-9]*", retry_log):
            raise RuntimeError("The slow IPv6 body path was not exercised")
        for family in ("ipv4", "ipv6"):
            if not re.search(
                rf"event=route_payload gid={delayed_gid} "
                rf"family={family} bytes=[1-9][0-9]*", retry_log
            ):
                raise RuntimeError(f"The delayed-family case did not receive {family} payload")
        delay = re.search(rf"event=range_retry gid={tail_gid} .*retry_in_ms=(\d+)",
                          retry_log)
        if not delay or int(delay[1]) < 10000:
            raise RuntimeError("Retry diagnostics omit the actual scheduled wait")

    return {"sha256": expected, "runs": results}


if __name__ == "__main__":
    raise SystemExit(run_validation("http", validate))
