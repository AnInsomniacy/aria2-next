# aria2-next

[![CI](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml/badge.svg)](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/AnInsomniacy/aria2-next.svg)](https://github.com/AnInsomniacy/aria2-next/releases)
[![License: GPLv2](https://img.shields.io/badge/license-GPLv2-blue.svg)](COPYING)

aria2-next is a maintained fork of [aria2](https://github.com/aria2/aria2), built as the download engine for [Motrix Next](https://github.com/AnInsomniacy/motrix-next) and other aria2-compatible integrations.

It keeps the familiar aria2 runtime surface: the executable is still `aria2c`, JSON-RPC remains aria2-compatible, and existing command-line options, configuration files, session files, and scripts are expected to keep working.

The fork modernizes the parts around the engine: CMake-only builds, current dependency baselines, reviewed reliability fixes, and reproducible release artifacts for macOS, Windows, Linux, and Android.

## Highlights

- Maintained aria2 fork with compatibility as the default.
- Built for Motrix Next as a native Tauri sidecar engine.
- CMake 3.25+ and Ninja as the maintained build path.
- Automated GitHub Releases for Linux, macOS, and Windows on x64 and ARM64.
- Android ARM64 packaging path retained for aria2-compatible mobile use.
- Updated release dependency baseline, including OpenSSL 3.5 LTS, c-ares 1.34, SQLite 3.53, Expat 2.8, and zlib 1.3.
- Preserved maintenance audit for upstream issue review and CMake migration work.

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

The downloaded binary is used the same way as aria2:

```bash
aria2c https://example.com/file.iso
aria2c --enable-rpc --rpc-listen-all=false --rpc-listen-port=6800
```

## Build From Source

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2c --version
```

A plain Ninja build without presets is also supported:

```bash
cmake -S . -B build/default -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/default
ctest --test-dir build/default --output-on-failure
```

Useful CMake options include `ARIA2_ENABLE_BITTORRENT`, `ARIA2_ENABLE_METALINK`, `ARIA2_ENABLE_WEBSOCKET`, `ARIA2_ENABLE_LIBARIA2`, `ARIA2_ENABLE_STATIC`, `ARIA2_WITH_APPLETLS`, `ARIA2_WITH_WINTLS`, `ARIA2_WITH_OPENSSL`, `ARIA2_WITH_GNUTLS`, `ARIA2_WITH_LIBXML2`, `ARIA2_WITH_EXPAT`, `ARIA2_WITH_CARES`, `ARIA2_WITH_ZLIB`, `ARIA2_WITH_SQLITE3`, and `ARIA2_WITH_LIBSSH2`.

## Compatibility

aria2-next is intended to remain compatible with aria2 consumers:

- `aria2c` stays the primary executable.
- CLI options and configuration names follow aria2.
- JSON-RPC method names and response shapes follow aria2.
- Session files and input files follow aria2 conventions.
- Public libaria2 headers remain under `src/includes/aria2/`.

Motrix Next uses this project as a sidecar binary, but the release artifacts are not Motrix-only binaries. They can be used by other tools that expect aria2-compatible behavior.

## What Changed From Upstream

The maintained tree has moved from Autotools to CMake. CMake is now the only supported build system for this repository.

Release packaging has also been rebuilt around reproducible GitHub Actions jobs. The release workflow builds six desktop targets, generates SHA-256 checksums, and uploads the assets to the published GitHub Release.

The `maintenance/` directory records the larger maintenance pass. The preserved issue matrix contains 137 upstream open bug issues that survived the bug-focused cleanup pass. Each retained issue has a final state, root-cause group, required action, and review evidence. Current preserved results include 44 issues in a `fixed-verified` state and 37 issues whose required action was `fixed-verified`.

## Release and Versioning

`CMakeLists.txt` is the project version source of truth. Release tags use `v{PROJECT_VERSION}`.

The normal release flow is:

1. Update the CMake project version.
2. Push the verified release commit.
3. Create and publish a GitHub Release for the matching tag.
4. Let `.github/workflows/release.yml` build and upload release assets.

Tag pushes alone do not publish release builds. `workflow_dispatch` remains available for manual release-path validation and uploads artifacts only to the workflow run.

## Dependency Baseline

Release dependency versions are tracked in [`packaging/dependencies.env`](packaging/dependencies.env).

| Dependency | Version | Release usage |
| --- | --- | --- |
| zlib | 1.3.2 | All release targets |
| Expat | 2.8.1 | Release targets using Expat |
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
| `CMakeLists.txt` | Root CMake build definition and project version source |
| `CMakePresets.json` | Standard configure, build, and test presets |
| `cmake/` | CMake templates, source inventories, and generated config inputs |
| `src/` | aria2 command-line client and core implementation |
| `src/includes/aria2/` | public libaria2 headers |
| `test/` | CppUnit test suite registered through CTest |
| `doc/` | manual, manpage, bash completion, and documentation tooling |
| `packaging/` | release dependencies, Dockerfiles, cross-build scripts, package assets |
| `third_party/` | vendored source with explicit ownership rules |
| `tools/` | repository helper scripts outside platform packaging |
| `maintenance/` | issue review records and CMake migration notes |

Directory-specific notes live in [`packaging/README.md`](packaging/README.md), [`tools/README.md`](tools/README.md), [`third_party/README.md`](third_party/README.md), and [`maintenance/README.md`](maintenance/README.md).

## License

Same as [aria2](https://github.com/aria2/aria2): [GPLv2](COPYING).
