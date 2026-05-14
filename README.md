# aria2-next

aria2-next is a maintained [aria2](https://github.com/aria2/aria2) fork focused on reliability fixes, current dependency baselines, and reproducible cross-platform releases.

The project keeps aria2's proven autotools build flow while organizing release packaging, third-party source, maintenance records, and helper tools under explicit ownership boundaries.

## Supported Platforms

| Platform | Arch | SSL/TLS | Linking |
|----------|------|---------|:-------:|
| Linux | x86_64 | OpenSSL | ✅ Fully static |
| Linux | ARM64 | OpenSSL | ✅ Fully static |
| macOS | ARM64 | AppleTLS | ✅ System frameworks only |
| macOS | x86_64 | AppleTLS | ✅ System frameworks only |
| Windows | x86_64 | Schannel | ✅ System DLLs only |
| Windows | ARM64 | Schannel | ✅ System DLLs only |

> **Note:** Official aria2 releases only ship Windows (x86/x64) and Android (ARM64) binaries.

## Features

All builds include the full feature set:

- Async DNS, BitTorrent, Metalink, XML-RPC
- HTTPS, SFTP (via libssh2)
- GZip, Message Digest, Firefox3 Cookie

## Usage

Download the latest binary from the project release page, extract it, and run:

```bash
chmod +x aria2c   # Linux/macOS only
./aria2c --version
```

## Build From Source

```bash
autoreconf -i
./configure
make
make check
```

Release packaging and cross-compilation helpers live under `packaging/`. The authoritative release dependency versions are stored in `packaging/dependencies.env`.

## Dependency Baseline

| Dependency | Version | Platforms |
|------------|---------|-----------|
| zlib | 1.3.2 | All |
| expat | 2.8.1 | All |
| c-ares | 1.34.6 | All |
| SQLite | 3.53.1 | All |
| libssh2 | 1.11.1 | All |
| OpenSSL | 3.5.6 LTS | Linux |
| GMP | 6.3.0 | macOS, Windows |
| libgpg-error | 1.61 | macOS |
| libgcrypt | 1.12.2 | macOS |

macOS binaries target `MACOSX_DEPLOYMENT_TARGET=11.0` (Big Sur+) for maximum compatibility.

## Repository Layout

| Path | Purpose |
|------|---------|
| `src/` | aria2 command-line client and core implementation |
| `src/includes/aria2/` | public libaria2 headers |
| `test/` | CppUnit test suite |
| `doc/` | English manual, manpage, bash completion, and documentation tooling |
| `packaging/` | maintained release, cross-build, Docker, and platform package assets |
| `third_party/` | bundled third-party source with explicit ownership notes |
| `tools/` | repository helper scripts that are not platform packaging |
| `maintenance/` | issue review records, modernization notes, and historical archives |
| `m4/` | checked-in autotools macros |
| `po/` | translation files and gettext workflow notes |
| `lib/` | libaria2 support build targets |

`README.md` is the only root readme. Legacy GNU placeholder files and generated documentation outputs are intentionally not kept in the repository.

See `packaging/README.md`, `tools/README.md`, `third_party/README.md`, and `maintenance/README.md` for directory-specific ownership rules.

## Maintenance Policy

New maintenance work should keep source behavior, packaging, dependency metadata, and documentation in sync. Dependency versions used by maintained release automation must be updated through `packaging/dependencies.env` first.

Historical or unsupported packaging assets should be moved under `packaging/legacy/` instead of remaining in the root directory.

## License

Same as [aria2](https://github.com/aria2/aria2): [GPLv2](COPYING).
