# aria2-builder

Cross-platform statically linked [aria2](https://github.com/aria2/aria2) builds — covering platforms beyond the official releases. All third-party dependencies compiled from source as static libraries.

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

Download the latest binary from [Releases](../../releases), extract, and run:

```bash
chmod +x aria2c   # Linux/macOS only
./aria2c --version
```

## Build Details

| Dependency | Version | Platforms |
|------------|---------|-----------|
| zlib | 1.3.1 | All |
| expat | 2.5.0 | All |
| c-ares | 1.19.1 | All |
| SQLite | 3.43.1 | All |
| libssh2 | 1.11.0 | All |
| OpenSSL | System | Linux |
| GMP | 6.3.0 | macOS, Windows |
| libgpg-error | 1.51 | macOS |
| libgcrypt | 1.10.3 | macOS |

macOS binaries target `MACOSX_DEPLOYMENT_TARGET=11.0` (Big Sur+) for maximum compatibility.

## Origin

Built as part of the [Motrix Next](https://github.com/AnInsomniacy/motrix-next) project. Binaries are universal and can be used by any project or standalone.

## License

Same as [aria2](https://github.com/aria2/aria2) — [GPLv2](COPYING).
