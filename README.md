# aria2-next

[![CI](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml/badge.svg)](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/AnInsomniacy/aria2-next.svg)](https://github.com/AnInsomniacy/aria2-next/releases)
[![License: GPLv2](https://img.shields.io/badge/license-GPLv2-blue.svg)](COPYING)

aria2-next is the maintained `aria2c` engine for [Motrix Next](https://github.com/AnInsomniacy/motrix-next) and other aria2-compatible consumers. It keeps the original aria2 command, configuration, session, JSON-RPC, and libaria2 interfaces intact while publishing current, reproducible, portable builds.

AnInsomniacy has maintained this fork since 2026. Maintenance focuses on cross-platform release reliability, dependency baselines, compatibility fixes, and a preserved audit of upstream issue history in [`docs/maintenance/issue-review-matrix.csv`](docs/maintenance/issue-review-matrix.csv).

## What This Repository Provides

| Area | Status |
| --- | --- |
| Engine | aria2-compatible `aria2c` binary |
| Primary consumer | Motrix Next sidecar engine |
| External consumers | Existing aria2 scripts, frontends, RPC clients, and automation |
| Build system | CMake 3.25+ with Ninja presets |
| Release targets | macOS, Windows, and Linux on x64 and ARM64 |
| Additional packaging | Android ARM64 |
| Maintenance | Maintained by AnInsomniacy since 2026 |
| Maintenance record | Preserved upstream issue review matrix |

## Compatibility

| Surface | Compatibility target |
| --- | --- |
| Executable | `aria2c` |
| CLI | aria2 option names and behavior |
| Configuration | aria2 config file format |
| Sessions | aria2 session and input file conventions |
| RPC | aria2 JSON-RPC methods and response shapes |
| Library | public libaria2 headers under `src/includes/aria2/` |

Motrix Next embeds this engine, but release artifacts are ordinary aria2-compatible binaries.

## Build

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2c --version
```

Plain Ninja builds are also supported:

```bash
cmake -S . -B build/default -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/default
ctest --test-dir build/default --output-on-failure
```

Common options include `ARIA2_ENABLE_BITTORRENT`, `ARIA2_ENABLE_METALINK`, `ARIA2_ENABLE_WEBSOCKET`, `ARIA2_ENABLE_LIBARIA2`, `ARIA2_ENABLE_STATIC`, `ARIA2_WITH_WINTLS`, `ARIA2_WITH_OPENSSL`, `ARIA2_WITH_GNUTLS`, `ARIA2_WITH_LIBXML2`, `ARIA2_WITH_EXPAT`, `ARIA2_WITH_CARES`, `ARIA2_WITH_ZLIB`, `ARIA2_WITH_SQLITE3`, and `ARIA2_WITH_LIBSSH2`.

Async DNS builds require c-ares 1.34.6 or newer.

## Downloads

Prebuilt artifacts are published on the [GitHub Releases](https://github.com/AnInsomniacy/aria2-next/releases) page.

| Platform | Architecture | Artifact |
| --- | --- | --- |
| Linux | x86_64 | `aria2c-<version>-linux-x86_64` |
| Linux | ARM64 | `aria2c-<version>-linux-aarch64` |
| macOS | Apple Silicon | `aria2c-<version>-macos-arm64` |
| macOS | Intel | `aria2c-<version>-macos-x86_64` |
| Windows | x86_64 | `aria2c-<version>-windows-x86_64.exe` |
| Windows | ARM64 | `aria2c-<version>-windows-arm64.exe` |
| Checksums | all release assets | `aria2c-<version>-checksums.sha256` |

Linux and macOS downloads are executable files. If your browser clears the executable bit, run `chmod +x ./aria2c-<version>-<platform>`.

Release binaries verify HTTPS certificates by default. Windows releases use WinTLS and the Windows trust store. OpenSSL and GnuTLS builds use system CA loading first, then a detected or explicitly configured CA bundle fallback.

Use the binary like aria2:

```bash
aria2c https://example.com/file.iso
aria2c --enable-rpc --rpc-listen-all=false --rpc-listen-port=6800
```

## Maintenance Audit

The durable audit artifacts live under [`docs/maintenance/`](docs/maintenance/). The preserved matrix contains 137 reviewed upstream bug issues, including 44 rows with final state `fixed-verified`.

The audit separates confirmed fixes, already-fixed reports, documented behavior, environment issues, platform issues, site-specific reports, non-reproducible reports, and larger architecture limitations.

## Release and Versioning

`CMakeLists.txt` is the project version source of truth. Release tags use `v{PROJECT_VERSION}`.

The release workflow runs when a matching GitHub Release is published. It validates the tag against `CMakeLists.txt`, builds all maintained platform binaries, generates SHA-256 checksums, and uploads the standalone executables to the published release. Source code is provided by the GitHub release tag source archives.

Tag pushes alone do not publish release builds. `workflow_dispatch` remains available for release-path validation of the current workflow commit and uploads artifacts only to the workflow run. Published GitHub Releases must use a `v{PROJECT_VERSION}` tag that matches `CMakeLists.txt`.

## Dependency Baseline

Release dependency versions are tracked in [`packaging/dependencies.env`](packaging/dependencies.env).
The same file records versions, archive names, download URLs, and SHA-256 hashes for source archives consumed by release workflows and Docker build contexts.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `CMakeLists.txt` | Project declaration and module entry point |
| `CMakePresets.json` | Standard configure, build, and test presets |
| `cmake/` | CMake modules, source inventories, and generated config templates |
| `src/` | aria2 command-line client and core implementation |
| `src/includes/aria2/` | public libaria2 headers |
| `tests/` | CppUnit test suite registered through CTest |
| `docs/` | manual sources, completion tooling, and maintenance records |
| `packaging/` | release dependencies, cross-build scripts, Dockerfiles, and package assets |
| `third_party/` | vendored source with explicit ownership rules |
| `tools/` | local developer helpers |

## License

Same as [aria2](https://github.com/aria2/aria2): [GPLv2](COPYING). The OpenSSL linking exception text is preserved in [`docs/licenses/OPENSSL.md`](docs/licenses/OPENSSL.md).
