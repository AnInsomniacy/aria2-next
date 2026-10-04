# Transfer validation

Manual integration tests exercise the executable through its CLI and JSON-RPC.
They are separate from CTest and never enter the engine runtime. Python 3.11+
is required. Local servers and controlled faults provide reproducible coverage;
public media sources provide independent interoperability checks.

| Module | Native test tools | Distinct coverage |
| --- | --- | --- |
| HTTP | Caddy, WireMock, Toxiproxy | Ranges, response filenames, redirects, conflicts, credentials, overload, recovery |
| SFTP | OpenSSH or SFTPGo, Toxiproxy | Authentication, host keys, interruption, recovery |
| BitTorrent | libtorrent | Native torrent creation, metadata, sharing, selection, recovery |
| ED2K | OpenSSL | Hashing, peer transfer, sharing, recovery |
| Metalink | WireMock | Mirrors, checksums, selection, failure handling |
| Media | FFmpeg, ffprobe, OpenSSL, Caddy, WireMock | HLS/DASH, origin-scoped headers, stable track IDs, encryption, clocks, recording, recovery |

Run local modules independently or sequentially:

```sh
python3 tools/transfer_validation/run.py media
python3 tools/transfer_validation/run.py all
```

Use `--engine PATH` to select a binary and `--keep-artifacts` to retain successful
local payloads. Failed runs always retain their state. Results, logs and payloads
live under `build/transfer-validation`. Shared helpers own process control,
hashing and native media inspection; protocol-specific scenarios remain separate.

For a quick executable check requiring only Python, run
`python3 tools/transfer_validation/http/smoke.py --engine PATH`. It downloads a
loopback fixture through both CLI and RPC, verifies file checksums and checks
clean shutdown. Use the packaged executable to catch build configuration defects
that a version query or a differently configured unit-test build cannot expose.

The dependency lock pins Caddy and Toxiproxy for Windows x64 and macOS ARM64.
WireMock requires Java 17+; Java 21 LTS is suitable. Other modules require their
listed tools; encryption and ED2K hashing use OpenSSL 3. Unpinned hosts fail
explicitly. Windows helpers run without opening console windows.

SFTP uses an isolated OpenSSH instance on POSIX and pinned SFTPGo portable mode
on Windows x64. Both bind to loopback and use fixture keys; SFTPGo creates only
an in-memory account rooted at the fixture directory. No system service or user
configuration is changed. The BitTorrent helper uses CMake's generated target
path, including the platform's executable suffix. Set `CMAKE_TOOLCHAIN_FILE` (or
`CC`/`CXX`) to match the engine's dependency build when compiling the helper.

Public media validation is opt-in and is excluded from `run.py all`:

```sh
python3 tools/transfer_validation/media/public.py
python3 tools/transfer_validation/media/public.py --case dash-live
python3 tools/transfer_validation/media/public.py --suite all
```

The public runner requires curl, FFmpeg and ffprobe. It snapshots the executable,
selects modest representations, fully decodes outputs, checks source content,
and retains every artifact. See the [sources and acceptance plan](../../docs/media-e2e.md).
