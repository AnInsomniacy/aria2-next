<div align="center">
  <img src="docs/media/banner.png" alt="Aria2 Next — Redefining the next generation of aria2" width="800" />
  <p>A download engine with aria2's command line and JSON-RPC, rebuilt on mature libraries.</p>

[![CI](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml/badge.svg)](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/AnInsomniacy/aria2-next.svg)](https://github.com/AnInsomniacy/aria2-next/releases)
[![License: GPLv2](https://img.shields.io/badge/license-GPLv2-blue.svg)](COPYING)

  <p>
    <img src="https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux%20%7C%20Android-blue.svg" alt="Platform: macOS, Windows, Linux, Android" />
  </p>
</div>

## Why Aria2 Next?

aria2 is remarkable open source software, and we are grateful to the authors
and contributors of the [aria2 project](https://github.com/aria2/aria2). Its
upstream development has slowed, its dependencies aged and many of its bugs
stayed open.

Aria2 Next keeps what users and frontends rely on — the `aria2` command line,
configuration files, session files, JSON-RPC and XML-RPC — and replaces the engine
behind them. Each protocol is handled by a library that owns it: libcurl for
HTTP(S) and SFTP, libtorrent for BitTorrent, GPAC and FFmpeg for HLS/DASH,
SQLite for state, Boost.Asio, Beast and JSON for the event loop and RPC. The
code of its own is limited to what no library provides: segment scheduling,
the task queue, ED2K/Kad, and the aria2-compatible surface. It is the engine
embedded in [Rayburst](https://github.com/AnInsomniacy/rayburst).

## Protocols

| Protocol | Implementation |
| --- | --- |
| HTTP, HTTPS | libcurl multi on Asio; adaptive byte ranges split by measured speed, endgame racing of slow tails, retry with `Retry-After`, resume validated by strong ETag or a Last-Modified that is at least 60 s older than the server's Date |
| SFTP | libcurl with libssh2 |
| BitTorrent, magnet | libtorrent-rasterbar 2.1: DHT, PEX, LSD, uTP, encryption, v1/v2, UPnP/NAT-PMP, file selection before download |
| ED2K, Kad | native, checked on the wire against aMule: server and Kad sources and search, hash sets, compressed and obfuscated transfers, upload queue and credits, UPnP port mapping |
| Metalink | version 4 (RFC 5854) |
| HLS, DASH | GPAC and FFmpeg: track selection, live recording, restartable segments, MP4/Matroska output without transcoding; see [media downloads](docs/media-downloads.md) |

Progress lives under `--state-dir`, not in `.aria2` files next to the
payload. A task resumes after a crash from its last checkpoint
(`--state-save-interval`), and a paused task shows its stored progress after a
restart.

## Compatibility

| Surface | Contract |
| --- | --- |
| Executable | `aria2-next` |
| Command line | aria2 options; retired options are mapped to their replacement or skipped with one warning, unknown options are errors |
| Configuration | aria2 `key=value` files |
| Sessions | aria2 input-file format |
| RPC | aria2 JSON-RPC over HTTP and WebSocket, aria2 XML-RPC over HTTP (`/rpc`), `system.multicall`, notifications, token secret, plus ED2K, BitTorrent and media [extensions](docs/rpc-extensions.md) |

Not provided: FTP, Metalink 3, JSONP and the libaria2 C++ library. Task state of
the original aria2 (`.aria2` control files) is not imported.

`--always-resume=false`, as passed by yt-dlp, is accepted and skipped with a
warning.

## Quick Start

```bash
aria2-next https://example.com/file.iso
aria2-next 'magnet:?xt=urn:btih:...'
aria2-next '<ed2k-file-link>'
aria2-next --enable-rpc --rpc-listen-port=6800
aria2-next --help=#all
```

## Build

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

The default superbuild compiles every library dependency from `third_party/`
without network access. CMake 3.25+, Ninja, a POSIX shell, Make, Perl and a
C11/C++20 toolchain are required; on Windows, MSYS2 supplies the shell, Make
and Perl while compilation uses the native target toolchain.

Build options: `ARIA2_ENABLE_WERROR`, `ARIA2_RELEASE_SIZE_OPTIMIZED`,
`ARIA2_RELEASE_LTO`. `tools/build_test.sh` builds and tests each of them.

Transfer behaviour is checked end to end by `tools/transfer_validation`
(HTTP against fault-injecting servers, SFTP, BitTorrent, ED2K, Metalink,
HLS/DASH) and `tools/bench` (throughput, tail latency, CPU and memory under
throttling, stalls, resets, overload and broken ranges). Contribution
guidelines are in [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md).

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
| Android 7.0+ | ARM64 | `aria2-next-<version>-android-arm64` |
| Checksums | all release assets | `aria2-next-<version>-checksums.sha256` |

Linux, macOS and Android downloads are executable files; if the executable bit
was cleared, run `chmod +x ./aria2-next-<version>-<platform>`. Linux binaries
require glibc 2.35 or newer.

On macOS and Linux, Homebrew installs the same binaries from the project's tap:

```bash
brew install aninsomniacy/aria2-next/aria2-next
```

Container images for Linux x86_64 and ARM64 are published to the GitHub
Container Registry:

```bash
docker run -d --name aria2-next \
  -p 6800:6800 \
  -e PUID="$(id -u)" \
  -e PGID="$(id -g)" \
  -v "$PWD/downloads:/downloads" \
  -v "$PWD/config:/config" \
  -v "$PWD/state:/var/lib/aria2-next" \
  ghcr.io/aninsomniacy/aria2-next:latest
```

or `PUID="$(id -u)" PGID="$(id -g)" docker compose -f packaging/docker/compose.yml up -d`.
The entrypoint assigns the writable mounts to `PUID`/`PGID` and creates the
configuration and session files when absent. Add `--rpc-secret=<token>` before
exposing RPC beyond a trusted network.

HTTPS certificates are verified by libcurl: Schannel and the system store on
Windows, SecTrust on macOS, OpenSSL default paths (honouring `SSL_CERT_FILE`
and `SSL_CERT_DIR`) on Linux and Android. `--ca-certificate` names a CA file
explicitly.

## Release and Versioning

`CMakeLists.txt` is the project version source of truth; release tags use
`v{PROJECT_VERSION}`. Publishing a GitHub Release builds all platform
binaries, uploads them with SHA-256 checksums, and then publishes the
container image for the same tag. Release dependency versions are tracked in
[`packaging/dependencies.env`](packaging/dependencies.env).

## Repository Layout

| Path | Purpose |
| --- | --- |
| `src/` | the engine; see the [architecture](docs/architecture.md) |
| `tests/` | doctest unit tests, registered with CTest |
| `cmake/` | CMake modules and the dependency superbuild |
| `docs/` | architecture, option reference, completion, maintenance records |
| `packaging/` | release dependencies, build scripts, container assets |
| `third_party/` | vendored dependency source |
| `tools/` | transfer validation, benchmark, build matrix |

The upstream issue audit is preserved in
[`docs/maintenance/upstream-issue-review/`](docs/maintenance/upstream-issue-review/).

## License

Same as [aria2](https://github.com/aria2/aria2): [GPLv2](COPYING). The OpenSSL
linking exception text is preserved in [`docs/licenses/OPENSSL.md`](docs/licenses/OPENSSL.md).
