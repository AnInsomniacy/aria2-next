# Support

Use this repository for aria2-next engine issues. Use the Motrix Next repository for desktop application issues.

## Where to Ask

Open an aria2-next issue for reproducible engine bugs, process crashes, CLI behavior regressions, JSON-RPC behavior regressions, release artifact problems, dependency build failures, protocol failures, checksum failures, certificate failures, proxy handling issues, and session or input-file regressions.

Open a Motrix Next issue for UI behavior, app preferences, installers, auto-update UI, tray behavior, history database behavior, notifications, browser extension flows, and desktop integration.

Use the question issue form for usage questions, configuration questions, integration questions, and reports that do not yet have a minimal reproduction.

## Minimal Engine Report

A useful report includes the aria2-next version, platform, architecture, binary source, exact command or JSON-RPC call, minimal input-file entry if applicable, debug log, expected behavior, actual behavior, and comparison with another version when this is a regression.

Run with debug logging where possible:

```bash
aria2-next --log-level=debug --console-log-level=debug --log=aria2-next.log ...
```

Redact RPC secrets, cookies, proxy credentials, authorization headers, private trackers, private magnets, and signed URL tokens before uploading logs.

## Motrix Next Boundary

Motrix Next starts aria2-next as a sidecar and talks to it through JSON-RPC. If the engine process crashes, rejects a task, returns a wrong RPC field, fails a transfer, or produces a protocol error outside Motrix, the root issue belongs here.

If the engine returns correct data but the app renders it incorrectly, the issue belongs in Motrix Next. When unsure, include the exported Motrix diagnostics and the aria2-next debug log so the boundary can be identified.

## External Services

Some failures are caused by remote services. Common examples are expired signed URLs, servers that ignore HTTP Range requests, mirrors that return invalid `Content-Range`, captive proxies, TLS interception, private tracker policy, and ED2K files with no reachable sources.

These reports are still useful when they expose an engine robustness issue. Include the observable server response or log line that identifies the failure mode.
