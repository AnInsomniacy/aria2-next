# aria2-next

aria2-next is a maintained [aria2](https://github.com/aria2/aria2) fork focused on reliability fixes, current dependency baselines, and reproducible cross-platform releases.

CMake is the only supported build system for this repository. Ninja is the default generator used by local development and release automation.

## Supported Platforms

| Platform | Arch | SSL/TLS | Linking |
|----------|------|---------|:-------:|
| Linux | x86_64 | OpenSSL | Static release build |
| Linux | ARM64 | OpenSSL | Static release build |
| macOS | ARM64 | AppleTLS or OpenSSL | System frameworks plus packaged dependencies |
| macOS | x86_64 | AppleTLS or OpenSSL | System frameworks plus packaged dependencies |
| Windows | x86_64 | Schannel or OpenSSL | System DLLs plus packaged dependencies |
| Windows | ARM64 | Schannel or OpenSSL | System DLLs plus packaged dependencies |
| Android | ARM64 | OpenSSL | Static release build |

## Features

Default builds enable Async DNS, BitTorrent, Metalink, XML-RPC, WebSocket, HTTPS, SFTP, GZip, message digests, and Firefox3/Chromium cookie support when the matching dependencies are available.

## Build From Source

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2c --version
```

A plain Ninja build without presets is also supported.

```bash
cmake -S . -B build/default -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/default
ctest --test-dir build/default --output-on-failure
```

Useful CMake options include `ARIA2_ENABLE_BITTORRENT`, `ARIA2_ENABLE_METALINK`, `ARIA2_ENABLE_WEBSOCKET`, `ARIA2_ENABLE_LIBARIA2`, `ARIA2_ENABLE_STATIC`, `ARIA2_WITH_APPLETLS`, `ARIA2_WITH_WINTLS`, `ARIA2_WITH_OPENSSL`, `ARIA2_WITH_GNUTLS`, `ARIA2_WITH_LIBXML2`, `ARIA2_WITH_EXPAT`, `ARIA2_WITH_CARES`, `ARIA2_WITH_ZLIB`, `ARIA2_WITH_SQLITE3`, and `ARIA2_WITH_LIBSSH2`.

Release packaging and cross-compilation assets live under `packaging/`. The authoritative release dependency versions are stored in `packaging/dependencies.env`.

## Dependency Baseline

| Dependency | Version | Platforms |
|------------|---------|-----------|
| zlib | 1.3.2 | All release targets |
| Expat | 2.8.1 | Release targets using Expat |
| c-ares | 1.34.6 | Async DNS release targets |
| SQLite | 3.53.1 | Cookie storage release targets |
| libssh2 | 1.11.1 | SFTP release targets |
| OpenSSL | 3.5.6 LTS | Linux, Windows, Android, optional macOS |
| GMP | 6.3.0 | macOS and Windows release dependency builds |
| libgpg-error | 1.61 | macOS release dependency builds |
| libgcrypt | 1.12.2 | macOS release dependency builds |
| Android NDK | r29 | Android release build |

## Repository Layout

| Path | Purpose |
|------|---------|
| `CMakeLists.txt` | Root CMake build definition |
| `CMakePresets.json` | Standard local configure, build, and test presets |
| `cmake/` | CMake templates and source inventories |
| `src/` | aria2 command-line client and core implementation |
| `src/includes/aria2/` | public libaria2 headers |
| `test/` | CppUnit test suite |
| `doc/` | manual, manpage, bash completion, and documentation tooling |
| `packaging/` | release, cross-build, Docker, and platform package assets |
| `third_party/` | bundled third-party source with explicit ownership notes |
| `tools/` | repository helper scripts that are not platform packaging |
| `maintenance/` | issue review records and migration notes |
| `lib/` | source support files used by the build |

See `packaging/README.md`, `tools/README.md`, `third_party/README.md`, and `maintenance/README.md` for directory-specific ownership rules.

## Maintenance Policy

New maintenance work should keep source behavior, CMake options, packaging, dependency metadata, and documentation in sync. Dependency versions used by maintained release automation must be updated through `packaging/dependencies.env` first.

Historical or unsupported packaging assets belong under `packaging/legacy/` with an explicit unsupported note.

## License

Same as [aria2](https://github.com/aria2/aria2): [GPLv2](COPYING).
