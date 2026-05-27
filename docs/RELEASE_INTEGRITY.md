# Release Integrity

This document describes how official aria2-next release artifacts are built and how users can verify them.

## Release Source

Official releases are created from Git tags named `v{PROJECT_VERSION}`. The tag version must match the version declared in `CMakeLists.txt`.

Publishing a GitHub Release triggers `.github/workflows/release.yml`. Tag pushes alone do not publish official release binaries.

## Build Scope

The release workflow builds maintained standalone binaries for Linux x86_64, Linux ARM64, macOS Apple Silicon, macOS Intel, Windows x86_64, and Windows ARM64. Android ARM64 release packaging is maintained through the repository packaging scripts.

Release jobs build the maintained dependency baseline recorded in `packaging/dependencies.env`, run local release smoke checks, verify runtime dependency closure, run size audits, and upload SHA-256 checksums with the final artifacts.

## Artifact Names

Official GitHub Release assets use these names:

```text
aria2-next-<version>-linux-x86_64
aria2-next-<version>-linux-aarch64
aria2-next-<version>-macos-arm64
aria2-next-<version>-macos-x86_64
aria2-next-<version>-windows-x86_64.exe
aria2-next-<version>-windows-arm64.exe
aria2-next-<version>-checksums.sha256
```

## Verification

After downloading an artifact and the checksum file, verify the checksum before execution:

```bash
sha256sum -c aria2-next-<version>-checksums.sha256
```

On macOS, use `shasum -a 256` when `sha256sum` is unavailable:

```bash
shasum -a 256 aria2-next-<version>-macos-arm64
```

Then inspect the binary version:

```bash
chmod +x ./aria2-next-<version>-<platform>
./aria2-next-<version>-<platform> --version
```

## Signing Status

aria2-next currently publishes checksummed standalone binaries. It does not currently publish notarized macOS applications, signed Windows installers, package-manager formulas, or auto-updater metadata.

If signing or package-manager distribution is added later, this document and the release workflow must be updated in the same change.

## Failed Releases

Published release tags are treated as immutable. If a release workflow fails before public consumption, maintainers may delete the failed GitHub Release and tag, fix the commit, rerun local verification, and recreate the same tag deliberately.

If a release has already been publicly consumed, maintainers must not silently replace the tag. A follow-up release is required.
