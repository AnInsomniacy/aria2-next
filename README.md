<div align="center">
  <img src="docs/media/banner.png" alt="Aria2 Next — Redefining the next generation of aria2" width="800" />
  <p>Maintained aria2 fork with extensive bug fixes and modernized architecture.</p>

[![CI](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml/badge.svg)](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/AnInsomniacy/aria2-next.svg)](https://github.com/AnInsomniacy/aria2-next/releases)
[![License: GPLv2](https://img.shields.io/badge/license-GPLv2-blue.svg)](COPYING)

  <p>
    <img src="https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux%20%7C%20Android-blue.svg" alt="Platform: macOS, Windows, Linux, Android" />
  </p>
</div>

## Why Aria2 Next?

aria2 is remarkable open source software. For over a decade it has been one of the most capable download engines available, trusted by countless tools and users worldwide. We are deeply grateful to the original authors and contributors of the [aria2 project](https://github.com/aria2/aria2). They built something that has stood the test of time, and that enduring quality is the best testament to their vision and craftsmanship.

But upstream development has slowed dramatically in recent years. Dependencies grew stale, builds broke on modern platforms, and a backlog of bugs went unaddressed. We picked up the baton: migrated the codebase to a modern build framework, triaged and fixed a substantial number of upstream issues, and introduced ED2K protocol support for the first time. A full audit trail is preserved in [`docs/maintenance/upstream-issue-review/matrix.csv`](docs/maintenance/upstream-issue-review/matrix.csv).

Aria2 Next is an actively maintained download engine and the embedded engine used by [Rayburst](https://github.com/AnInsomniacy/rayburst). HTTP, HTTPS, SFTP, Metalink, ED2K, RPC, and libaria2 remain supported. HTTP, HTTPS, SFTP, and Metalink payload transfers use libcurl 8.21 with nghttp2. BitTorrent uses libtorrent-rasterbar 2.1. FTP and the former custom stream protocol stacks have been removed.

Stream, BitTorrent, and ED2K resume state is stored under `--state-dir`. Payload directories no longer receive adjacent `.aria2` control files. HTTP cookies, authentication, proxies, TLS, SFTP, resume validation, and HTTP/2 use their native library implementations. HTTP and HTTPS transfers use libcurl's event-driven multi-socket API, adaptive verified byte ranges, HTTP/2 multiplexing, shared cookies and TLS sessions, and range-aware SQLite resume state.

Automatic stream filenames decode the URL basename once using libcurl. Explicit
`--out` values and metadata-defined paths remain literal; request URLs are unchanged.

HTTP range transfers retain unreturned portions of valid partial responses and
retry only unfinished data. Connection setup and low-speed timeouts use libcurl;
server overload reduces new request concurrency and staggers retries. Slow tail
ranges are split only when the remaining work justifies another request. Paused
stream tasks restore their saved progress before resuming network activity.
HTTP 403 does not lower the concurrency budget. HTTP 429/503 reduces it once
per request generation; subsequent payload progress restores concurrency
without waiting for an entire range to complete.

Validated redirect destinations are reused for subsequent ranges. Expired
destinations are refreshed through one request per source route instead of sending
every range back through a signed entry point. Credentials retain their
original origin boundary, and cookies remain managed by libcurl.

Unrestricted parallel HTTP downloads use a bounded set of workers across
available address families. Workers retain their transport choice while taking
ranges from one queue; faster workers complete more work without changing the
path of other active transfers. Redirect destinations belong to their source
route. Received bytes stay committed to the same range planner, and idle workers
can assist unfinished ranges. DNS, connection establishment, TLS, and pooling
remain native libcurl operations. Explicit IPv4, interface, proxy, and rate-limit
settings are respected.

Magnet downloads keep one GID from metadata discovery through file selection, payload transfer, and seeding. With `pause-metadata=true`, the same GID remains paused with a complete file list and `bittorrent.fileSelectionState=awaiting` until a valid `select-file` is submitted. Aria2 Next then replaces the metadata-only native handle with a checked libtorrent handle that already contains the final file priorities. This prevents stale partfiles from entering the payload session.

The BitTorrent session persists native IPv4 and IPv6 DHT routing state, restores paused torrents into libtorrent without activating network transfer, checkpoints fast-resume data while running, preserves representable tracker tiers, compacts only excess lowest-priority tiers, and isolates private torrents from global tracker injection. Paused tasks retain verified task and file progress across process restarts. Permanent task removal deletes fast-resume and partfile state as one native operation. TCP, uTP, PEX, Local Peer Discovery, UPnP/NAT-PMP, transport encryption, v1/v2 torrents, sparse or allocated storage, sequential mode, and HTTP/SOCKS proxying use native libtorrent facilities.

Advanced native controls cover request and disk queues, disk I/O policy, peer turnover, TCP/uTP balancing, upload scheduling, file priorities, web seeds, manual peers, and resume checkpoints. Runtime diagnostics report peer discovery, tracker, DHT, transport, disk queue, protocol overhead, and aggregated performance-warning state through JSON-RPC.

## Native HLS and DASH Downloads

HLS (`.m3u8`) and MPEG-DASH (`.mpd`) use native GPAC, libcurl, and FFmpeg
libraries inside the same engine. Media tasks support representation selection,
live recording, paused restart, and MP4/Matroska output without transcoding.
Media progress uses presentation duration rather than an invented output size.
See [media downloads](docs/media-downloads.md) for CLI/RPC contracts and scope.

## Native ED2K/eMule Support

Aria2 Next includes native ED2K/eMule support aligned with aMule's network behavior. ED2K works through the normal aria2 CLI, session, and JSON-RPC flows, including server and Kad discovery, peer transfer, search, sharing, upload queues, protocol obfuscation, persistent runtime identity, and restart source seeds. The maintained architecture and verification scope are documented in [`docs/maintenance/ed2k-runtime.md`](docs/maintenance/ed2k-runtime.md).

## Compatibility

| Surface | Current contract |
| --- | --- |
| Executable | `aria2-next` |
| CLI | Maintained aria2-next options with retired aria2 input normalization |
| Configuration | aria2 key-value format with retired input normalization |
| Sessions | Canonical aria2-next task files and native engine resume data |
| RPC | Maintained aria2 JSON-RPC methods, retired option normalization, and native extensions |
| Library | Public libaria2 headers with retired option normalization |

Rayburst embeds this engine, and release artifacts are standalone binaries.

Linux release binaries require glibc 2.35 or newer and a compatible system C++
runtime, matching the Ubuntu 22.04 build and container baseline.
Recognized retired aria2 options are mapped when a current equivalent exists or
skipped with one warning when the native engine owns or removed the behavior.
Unknown option names remain errors. Legacy task state and adjacent `.aria2`
control files are not imported.

`always-resume` is accepted as a retired boolean option, including the
`--always-resume=false` argument supplied by yt-dlp. It is skipped with a warning;
native recovery ownership, bounded full-response restarts and explicit overwrite
settings determine the download behavior.

## Quick Start

Download a file:

```bash
aria2-next https://example.com/file.iso
```

Download an ED2K file link:

```bash
aria2-next '<ed2k-file-link>'
```

Run the JSON-RPC server:

```bash
aria2-next --enable-rpc --rpc-listen-all=false --rpc-listen-port=6800
```

Run the JSON-RPC server as a daemon:

```bash
aria2-next --daemon=true --enable-rpc=true --rpc-listen-all=false --rpc-listen-port=6800
```

Inspect enabled features and build details:

```bash
aria2-next --version
aria2-next --help=#ed2k
```

## What This Repository Provides

| Area | Status |
| --- | --- |
| Engine | Multi-protocol `aria2-next` binary |
| Primary consumer | Rayburst sidecar engine |
| External consumers | Existing aria2 scripts, frontends, RPC clients, and automation |
| Build system | CMake 3.25+ with Ninja presets |
| Release targets | macOS, Windows, Linux, and Android on maintained CPU architectures |
| Maintenance | Maintained by AnInsomniacy since 2026 |
| Maintenance record | Preserved upstream issue review matrix |

## Build

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

Contribution guidelines are available in [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md).

Plain Ninja builds are also supported:

```bash
cmake -S . -B build/default -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/default
ctest --test-dir build/default --output-on-failure
```

Common options include `ARIA2_ENABLE_BITTORRENT`, `ARIA2_ENABLE_METALINK`, `ARIA2_ENABLE_WEBSOCKET`, `ARIA2_ENABLE_LIBARIA2`, `ARIA2_RELEASE_SIZE_OPTIMIZED`, and `ARIA2_RELEASE_LTO`.

The default superbuild compiles every library dependency from `third_party`.
Only CMake, Ninja, a POSIX shell, Make, Perl, and a C11/C++17 platform toolchain
are required. On Windows, MSYS2 supplies the shell, Make, and Perl; compilation
uses the native target toolchain.

## Downloads

Prebuilt artifacts are published on the [GitHub Releases](https://github.com/AnInsomniacy/aria2-next/releases) page.

| Platform | Architecture | Artifact |
| --- | --- | --- |
| Linux | x86_64 | `aria2-next-<version>-linux-x86_64` |
| Linux | ARM64 | `aria2-next-<version>-linux-aarch64` |
| macOS | Apple Silicon | `aria2-next-<version>-macos-arm64` |
| macOS | Intel | `aria2-next-<version>-macos-x86_64` |
| Windows | x86_64 | `aria2-next-<version>-windows-x86_64.exe` |
| Windows | ARM64 | `aria2-next-<version>-windows-arm64.exe` |
| Android | ARM64 | `aria2-next-<version>-android-arm64` |
| Checksums | all release assets | `aria2-next-<version>-checksums.sha256` |

Linux, macOS, and Android downloads are executable files. If your browser clears the executable bit, run `chmod +x ./aria2-next-<version>-<platform>`.

Container images are published to GitHub Container Registry for Linux x86_64 and Linux ARM64.

Run the latest container:

```bash
docker run --rm \
  -e PUID="$(id -u)" \
  -e PGID="$(id -g)" \
  ghcr.io/aninsomniacy/aria2-next:latest --version
```

Run as a JSON-RPC service:

```bash
docker run -d --name aria2-next \
  -p 6800:6800 \
  -e PUID="$(id -u)" \
  -e PGID="$(id -g)" \
  -v "$PWD/downloads:/downloads" \
  -v "$PWD/config:/config" \
  -v "$PWD/state:/var/lib/aria2-next" \
  --memory=512m --cpus=2 --pids-limit=256 \
  ghcr.io/aninsomniacy/aria2-next:latest
```

Run the maintained Compose definition with the host account IDs:

```bash
PUID="$(id -u)" PGID="$(id -g)" \
  docker compose -f packaging/docker/compose.yml up -d
```

The container requires `PUID` and `PGID`. The entrypoint assigns writable mounts to those IDs and creates the configuration and session files when absent. Non-root IDs are recommended. UID or GID `0` is supported for environments that require elevated access and emits a startup warning. The default configuration enables JSON-RPC inside the container, stores downloads in `/downloads`, and keeps session state in `/var/lib/aria2-next`. Add `--rpc-secret=<token>` before exposing RPC beyond a trusted local network.

Release binaries verify HTTPS certificates through libcurl by default. Windows uses Schannel and the native certificate store; unavailable revocation services are treated as a soft failure while explicitly revoked certificates remain rejected. macOS uses Apple SecTrust. Linux and Android use OpenSSL default trust paths and honor `SSL_CERT_FILE` and `SSL_CERT_DIR`. Explicit CA files remain available through `--ca-certificate`.

## Maintenance Audit

The durable audit artifacts live under [`docs/maintenance/`](docs/maintenance/). The preserved matrix contains 137 reviewed upstream bug issues, including 43 rows with final state `fixed-verified`.

The audit separates confirmed fixes, already-fixed reports, documented behavior, environment issues, platform issues, site-specific reports, non-reproducible reports, and larger architecture limitations.

## Release and Versioning

`CMakeLists.txt` is the project version source of truth. Release tags use `v{PROJECT_VERSION}`.

The release workflow runs when a matching GitHub Release is published. It validates the tag against `CMakeLists.txt`, builds all maintained platform binaries, generates SHA-256 checksums, uploads the release executables, and then dispatches Docker publishing for the same release tag. Source code is provided by the GitHub release tag source archives.

Tag pushes alone do not publish release builds. `workflow_dispatch` remains available for release-path validation of the current workflow commit and archives the final binaries and checksum file to the workflow run artifact named `aria2-next-<version>-release-assets`. Published GitHub Releases must use a `v{PROJECT_VERSION}` tag that matches `CMakeLists.txt`.

Official container images are published automatically after the release assets upload succeeds. The Docker Publish workflow remains manually dispatchable for an independent rebuild and uses the latest GitHub Release when no tag is supplied. It assembles the Linux multi-architecture image from release binaries and publishes `ghcr.io/aninsomniacy/aria2-next:latest` plus the matching `v{PROJECT_VERSION}` tag.

## Dependency Baseline

Release dependency versions are tracked in [`packaging/dependencies.env`](packaging/dependencies.env).
The corresponding source is stored under [`third_party/`](third_party/), so library builds require no network access.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `CMakeLists.txt` | Project declaration and module entry point |
| `CMakePresets.json` | Standard configure, build, and test presets |
| `cmake/` | CMake modules, source inventories, and generated config templates |
| `src/` | Aria2 Next command-line client and core implementation; see the [ownership map](docs/architecture.md) |
| `src/includes/aria2/` | public libaria2 headers |
| `tests/` | doctest unit test suite registered through CTest |
| `docs/` | manual sources, completion tooling, and maintenance records |
| `packaging/` | release dependencies, cross-build scripts, and package assets |
| `third_party/` | vendored source with explicit ownership rules |
| `tools/` | local developer helpers |

## License

Same as [aria2](https://github.com/aria2/aria2): [GPLv2](COPYING). The OpenSSL linking exception text is preserved in [`docs/licenses/OPENSSL.md`](docs/licenses/OPENSSL.md).
