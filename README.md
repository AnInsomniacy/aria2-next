<div align="center">
  <img src="docs/media/aria2-next-icon.png" alt="Aria2 Next icon" width="144" height="144" />
  <h1>Aria2 Next</h1>
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

Aria2 Next is an actively maintained aria2-compatible engine for everyone, and it is also the embedded engine used by [Motrix Next](https://github.com/AnInsomniacy/motrix-next). Original interfaces, including options, configuration, sessions, JSON-RPC, and libaria2, remain intact so downstream projects get a seamless upgrade. The focus is straightforward: release reliability, current dependency baselines, and ongoing compatibility fixes. Same engine, renewed foundation.

## Native ED2K/eMule Support

Aria2 Next includes native ED2K/eMule support reimplemented inside aria2's existing engine architecture from authoritative eMule, aMule, MLDonkey, Wireshark, and protocol documentation references. ED2K works through normal aria2-style CLI, session, and JSON-RPC flows, including source discovery, peer transfer, search, task-level sharing, upload cooperation, queue maintenance, and Motrix Next integration surfaces. The reference-alignment and download-hardening work is tracked in [`docs/maintenance/ed2k-refactor/`](docs/maintenance/ed2k-refactor/) and [`docs/maintenance/ed2k-download-hardening/`](docs/maintenance/ed2k-download-hardening/), with obsolete legacy structures removed or replaced by aria2-next-native mechanisms.

## Compatibility

| Surface | Compatibility target |
| --- | --- |
| Executable | `aria2-next` |
| CLI | aria2 option names and behavior |
| Configuration | aria2 config file format |
| Sessions | aria2 session and input file conventions |
| RPC | aria2 JSON-RPC methods and response shapes |
| Library | public libaria2 headers under `src/includes/aria2/` |

Motrix Next embeds this engine, but release artifacts are ordinary aria2-compatible binaries.

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

Inspect enabled features and build details:

```bash
aria2-next --version
aria2-next --help=#ed2k
```

## What This Repository Provides

| Area | Status |
| --- | --- |
| Engine | aria2-compatible `aria2-next` binary |
| Primary consumer | Motrix Next sidecar engine |
| External consumers | Existing aria2 scripts, frontends, RPC clients, and automation |
| Build system | CMake 3.25+ with Ninja presets |
| Release targets | macOS, Windows, and Linux on x64 and ARM64 |
| Additional packaging | Android ARM64 release builds |
| Maintenance | Maintained by AnInsomniacy since 2026 |
| Maintenance record | Preserved upstream issue review matrix |

## Build

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

Plain Ninja builds are also supported:

```bash
cmake -S . -B build/default -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/default
ctest --test-dir build/default --output-on-failure
```

Common options include `ARIA2_ENABLE_BITTORRENT`, `ARIA2_ENABLE_METALINK`, `ARIA2_ENABLE_WEBSOCKET`, `ARIA2_ENABLE_LIBARIA2`, `ARIA2_STATIC_DEPENDENCIES`, `ARIA2_RELEASE_SIZE_OPTIMIZED`, `ARIA2_RELEASE_LTO`, `ARIA2_WITH_WINTLS`, `ARIA2_WITH_OPENSSL`, `ARIA2_WITH_GNUTLS`, `ARIA2_WITH_LIBXML2`, `ARIA2_WITH_EXPAT`, `ARIA2_WITH_CARES`, `ARIA2_WITH_ZLIB`, `ARIA2_WITH_SQLITE3`, and `ARIA2_WITH_LIBSSH2`.

Async DNS builds require c-ares 1.34.6 or newer.

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
| Checksums | all release assets | `aria2-next-<version>-checksums.sha256` |

Linux and macOS downloads are executable files. If your browser clears the executable bit, run `chmod +x ./aria2-next-<version>-<platform>`.

Release binaries verify HTTPS certificates by default. Windows releases use WinTLS and the Windows trust store. Linux OpenSSL builds use the system OpenSSL 3 runtime so certificate discovery follows the host distribution. macOS OpenSSL and GnuTLS builds use their backend's system trust loading. Explicit CA files remain available through `--ca-certificate`.

## Maintenance Audit

The durable audit artifacts live under [`docs/maintenance/`](docs/maintenance/). The preserved matrix contains 137 reviewed upstream bug issues, including 43 rows with final state `fixed-verified`.

The audit separates confirmed fixes, already-fixed reports, documented behavior, environment issues, platform issues, site-specific reports, non-reproducible reports, and larger architecture limitations.

## Release and Versioning

`CMakeLists.txt` is the project version source of truth. Release tags use `v{PROJECT_VERSION}`.

The release workflow runs when a matching GitHub Release is published. It validates the tag against `CMakeLists.txt`, builds all maintained platform binaries, generates SHA-256 checksums, and uploads the release executables to the published release. Source code is provided by the GitHub release tag source archives.

Tag pushes alone do not publish release builds. `workflow_dispatch` remains available for release-path validation of the current workflow commit and uploads the final binaries and checksum file only to the workflow run artifact named `aria2-next-<version>-release-assets`. Published GitHub Releases must use a `v{PROJECT_VERSION}` tag that matches `CMakeLists.txt`.

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
