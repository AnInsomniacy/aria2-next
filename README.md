# aria2-next

[![CI](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml/badge.svg)](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/AnInsomniacy/aria2-next.svg)](https://github.com/AnInsomniacy/aria2-next/releases)
[![License: GPLv2](https://img.shields.io/badge/license-GPLv2-blue.svg)](COPYING)

aria2-next is the maintained [aria2](https://github.com/aria2/aria2) fork used as the `aria2c` sidecar engine for [Motrix Next](https://github.com/AnInsomniacy/motrix-next).

It is maintained from the upstream aria2 codebase with a large issue-review pass, targeted reliability fixes, a CMake-only build, current release dependencies, and reproducible binaries for macOS, Windows, Linux, and Android.

The runtime surface stays aria2-compatible for non-Motrix users. The executable name, CLI options, configuration files, session files, JSON-RPC API, and libaria2 headers keep the original aria2 format unless a change is explicitly documented.

## Relationship to Motrix Next

[Motrix Next](https://github.com/AnInsomniacy/motrix-next) runs aria2 as a native Tauri sidecar process. This repository supplies that engine.

The sidecar relationship is the main reason this fork exists. Motrix Next needs current, reproducible aria2 binaries across its supported platforms, including macOS Apple Silicon, macOS Intel, Windows x64, Windows ARM64, Linux x64, and Linux ARM64.

The binaries are not locked to Motrix Next. They remain ordinary aria2-compatible `aria2c` builds and can be used by scripts, frontends, RPC clients, and automation that already know how to use aria2.

## Compatibility

aria2-next follows the aria2 contract by default.

| Surface | Compatibility expectation |
| --- | --- |
| Executable | `aria2c` remains the primary binary |
| CLI | Existing aria2 options and option names remain the baseline |
| Configuration | Existing aria2 config files remain usable |
| Sessions | Existing aria2 session and input file conventions remain usable |
| RPC | JSON-RPC method names and response shapes follow aria2 |
| Library | Public libaria2 headers remain under `src/includes/aria2/` |

Compatibility-sensitive changes should be rare, intentional, tested, and documented.

## Maintenance Work

This fork includes a large upstream issue review pass.

The maintenance work started from the upstream aria2 issue backlog of more than 1,000 issues. After cleanup and triage, the actionable bug-focused set was reduced and reviewed in detail. The durable record preserved in this repository is [`maintenance/issue-review-matrix.csv`](maintenance/issue-review-matrix.csv).

The preserved matrix contains 137 reviewed upstream open bug issues. Each row records the issue number, priority, affected module, title, final state, root-cause group, required action, and review evidence.

Current preserved results include:

- 44 issues with final state `fixed-verified`.
- 37 issues whose required action was `fixed-verified`.
- Reviewed classifications for reports that were already fixed, documented behavior, environment-specific, site-specific, platform-specific, not reproducible, or too large for a small isolated patch.

The matrix is intentionally conservative. It separates fixes from non-code decisions so the repository keeps an auditable record instead of turning every upstream report into speculative code changes.

## Build System Modernization

aria2-next has moved the maintained build path from Autotools to CMake.

CMake is now the only supported build system for this repository. Release packaging, local development, tests, and cross-platform automation all build this checkout through CMake. Ninja is the default generator.

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

## What This Repository Provides

- An aria2-compatible `aria2c` binary for Motrix Next and other integrations.
- Six desktop release targets: macOS, Windows, and Linux on x64 and ARM64.
- Android ARM64 packaging support for aria2-compatible mobile use.
- A maintained CMake 3.25+ build system with Ninja as the default generator.
- A documented release dependency baseline for reproducible packaging.
- A preserved maintenance audit of upstream issue review and fix decisions.

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

Use the downloaded binary as a normal aria2 binary:

```bash
aria2c https://example.com/file.iso
aria2c --enable-rpc --rpc-listen-all=false --rpc-listen-port=6800
```

## Release and Versioning

`CMakeLists.txt` is the project version source of truth. Release tags use `v{PROJECT_VERSION}`.

The release workflow is designed around GitHub Releases:

1. The release commit carries the target CMake project version.
2. The GitHub Release is created and published for the matching `v<version>` tag.
3. `.github/workflows/release.yml` validates the tag against `CMakeLists.txt`.
4. The workflow builds all maintained platform artifacts.
5. The workflow generates SHA-256 checksums and uploads assets to the published GitHub Release.

Tag pushes alone do not publish release builds. `workflow_dispatch` is retained for manual release-path validation and uploads artifacts only to the workflow run.

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
