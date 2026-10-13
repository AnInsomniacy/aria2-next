# Packaging

This directory owns release packaging, cross-compilation helpers, container resources, and release dependency metadata.

`dependencies.env` is the authoritative version baseline for vendored libraries and maintained release toolchains. Library source is stored under `third_party/`; release jobs do not download it.

## Layout

Linux releases target Ubuntu 22.04 and require glibc 2.35 or newer, with a
compatible system C++ runtime. The Docker image uses the same baseline.
Runtime checks inspect ELF version requirements with `readelf` before upload.

Windows builds target Windows 10 or later. CMake supplies the same `WINVER` and
`_WIN32_WINNT` definitions to the engine and its consumers to match libtorrent;
header include order must not select incompatible Boost.Asio lock layouts.

| Path | Purpose |
| --- | --- |
| `docker/` | Linux runtime container image definition |
| `openssl/` | Maintained OpenSSL target configuration |
| `scripts/` | Release packaging helpers |
| `dependencies.env` | Maintained library and toolchain version baseline |

Supported packaging paths build this repository checkout and its vendored library source without network dependency resolution.

The shared CMake superbuild resolves compiler and binary tools to absolute
paths before configuring dependencies. OpenSSL is installed through a staged,
stable filesystem prefix so release binaries contain no runner-specific build
paths. Platform toolchain settings, deployment targets, and Android API levels
are applied consistently to every dependency.

Official release builds use `packaging/scripts/release-size-profile` to apply size-oriented compiler flags, per-function and per-data sections, and platform linker dead-code elimination. The profile is used by GitHub release jobs so portable artifacts keep the maintained dependency baseline without retaining avoidable unused code.

GitHub Release assets are bare executable binaries named `aria2-next-<version>-<platform>-<architecture>`, plus a SHA-256 checksum file. Source code and license material are provided by the GitHub release tag source archives.

Container images are published automatically after official release assets are uploaded and remain independently publishable through the manual Docker workflow. GitHub Container Registry receives `ghcr.io/aninsomniacy/aria2-next:v<version>` and `ghcr.io/aninsomniacy/aria2-next:latest`. The image is assembled from the selected GitHub Release Linux x86_64 and Linux ARM64 binaries instead of rebuilding aria2-next inside Docker. `PUID` and `PGID` are mandatory. The entrypoint assigns persistent mounts to those IDs and seeds the configuration and session state. Non-root IDs are recommended; UID or GID `0` remains available with an explicit startup warning.

`packaging/scripts/check-runtime-deps` and `packaging/scripts/size-audit` remain available for manual release inspection.

The release dependency boundary is platform-specific. Vendored libraries are built for every release target and linked statically. Linux binaries may use the system ELF loader and C/C++ runtime. macOS binaries may link only Apple system libraries and frameworks. Windows HTTPS uses libcurl with Schannel, secure RPC uses statically linked OpenSSL, and release binaries may link only Windows system DLLs. Android binaries may link only Android system runtime libraries and must not require `libc++_shared.so`.

Maintained libtorrent builds explicitly enable DHT, peer encryption, protocol extensions, mutable torrents, and streaming priorities. I2P and WebTorrent are disabled because aria2-next does not expose those transports and the release artifacts do not ship their runtime dependencies.
