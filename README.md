# aria2-next

[![CI](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml/badge.svg)](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/AnInsomniacy/aria2-next.svg)](https://github.com/AnInsomniacy/aria2-next/releases)
[![License: GPLv2](https://img.shields.io/badge/license-GPLv2-blue.svg)](COPYING)

aria2-next is a maintained fork of [aria2](https://github.com/aria2/aria2) and the `aria2c` sidecar engine used by [Motrix Next](https://github.com/AnInsomniacy/motrix-next). It keeps the original aria2 command, configuration, session, JSON-RPC, and libaria2 interfaces intact, so other aria2-compatible applications can use these builds directly.

The fork modernizes the project around a CMake-only build, reproducible multi-platform releases, and a preserved maintenance audit. The audit worked through the upstream issue history, cleaned it into an actionable bug set, and records the reviewed decisions in [`docs/maintenance/issue-review-matrix.csv`](docs/maintenance/issue-review-matrix.csv).

## What This Repository Provides

| Area | Status |
| --- | --- |
| Engine | aria2-compatible `aria2c` binary |
| Primary consumer | Motrix Next sidecar engine |
| External consumers | Existing aria2 scripts, frontends, RPC clients, and automation |
| Build system | CMake 3.25+ with Ninja presets |
| Release targets | macOS, Windows, and Linux on x64 and ARM64 |
| Additional packaging | Android ARM64 |
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

Common options include `ARIA2_ENABLE_BITTORRENT`, `ARIA2_ENABLE_METALINK`, `ARIA2_ENABLE_WEBSOCKET`, `ARIA2_ENABLE_LIBARIA2`, `ARIA2_ENABLE_STATIC`, `ARIA2_WITH_APPLETLS`, `ARIA2_WITH_WINTLS`, `ARIA2_WITH_OPENSSL`, `ARIA2_WITH_GNUTLS`, `ARIA2_WITH_LIBXML2`, `ARIA2_WITH_EXPAT`, `ARIA2_WITH_CARES`, `ARIA2_WITH_ZLIB`, `ARIA2_WITH_SQLITE3`, and `ARIA2_WITH_LIBSSH2`.

## Downloads

Prebuilt artifacts are published on the [GitHub Releases](https://github.com/AnInsomniacy/aria2-next/releases) page.

| Platform | Architecture | Artifact |
| --- | --- | --- |
| Linux | x86_64 | `aria2-<version>-linux-x86_64.tar.xz` |
| Linux | ARM64 | `aria2-<version>-linux-aarch64.tar.xz` |
| macOS | Apple Silicon | `aria2-<version>-macos-arm64.tar.bz2` |
| macOS | Intel | `aria2-<version>-macos-x86_64.tar.bz2` |
| Windows | x86_64 | `aria2-<version>-windows-x86_64.zip` |
| Windows | ARM64 | `aria2-<version>-windows-arm64.zip` |
| Checksums | all release assets | `aria2-<version>-checksums.sha256` |

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

The release workflow runs when a matching GitHub Release is published. It validates the tag against `CMakeLists.txt`, builds all maintained platform artifacts, generates SHA-256 checksums, and uploads assets to the published release.

Tag pushes alone do not publish release builds. `workflow_dispatch` remains available for release-path validation and uploads artifacts only to the workflow run.

## Dependency Baseline

Release dependency versions are tracked in [`packaging/dependencies.env`](packaging/dependencies.env).

| Dependency | Version | Release usage |
| --- | --- | --- |
| zlib | 1.3.2 | All release targets |
| Expat | 2.8.1 | XML parser release targets |
| c-ares | 1.34.6 | Async DNS release targets |
| SQLite | 3.53.1 | Cookie storage release targets |
| libssh2 | 1.11.1 | SFTP release targets |
| OpenSSL | 3.5.6 LTS | Linux, Windows, Android, optional macOS |
| GMP | 6.3.0 | macOS and Windows dependency builds |
| libgpg-error | 1.61 | macOS dependency builds |
| libgcrypt | 1.12.2 | macOS dependency builds |
| Android NDK | r29 | Android release build |

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
