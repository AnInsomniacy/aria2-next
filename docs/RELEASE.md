# Release Guide

This document covers official Aria2 Next releases, user verification, maintainer release flow, manual debug artifacts, and failed-release recovery.

## Version Source

`CMakeLists.txt` is the only project version source. Release tags use `v{PROJECT_VERSION}` and must match the CMake version after removing the leading `v`.

Pre-release, beta, RC, channel suffix, build metadata, and date-based release suffixes are not supported for Aria2 Next releases.

## Official Assets

Official GitHub Releases publish standalone executable assets:

| Platform | Asset |
| --- | --- |
| Linux x86_64 | `aria2-next-<version>-linux-x86_64` |
| Linux ARM64 | `aria2-next-<version>-linux-aarch64` |
| macOS Apple Silicon | `aria2-next-<version>-macos-arm64` |
| macOS Intel | `aria2-next-<version>-macos-x86_64` |
| Windows x86_64 | `aria2-next-<version>-windows-x86_64.exe` |
| Windows ARM64 | `aria2-next-<version>-windows-arm64.exe` |
| Checksums | `aria2-next-<version>-checksums.sha256` |

GitHub source archives provide the source tree and license material. Android ARM64 packaging is maintained through repository scripts, but it is not part of the standard desktop release asset set.

## Verification

Download the binary and checksum file from the same GitHub Release, then verify the checksum before execution:

```bash
sha256sum -c aria2-next-<version>-checksums.sha256
```

On macOS:

```bash
shasum -a 256 aria2-next-<version>-macos-arm64
```

Then inspect the binary:

```bash
chmod +x ./aria2-next-<version>-<platform>
./aria2-next-<version>-<platform> --version
```

Aria2 Next currently publishes checksummed standalone binaries. It does not publish notarized macOS apps, signed Windows installers, package-manager formulas, or updater metadata.

## Workflow Behavior

The release workflow runs on `release: published`. Pushing a tag does not publish binaries.

Manual `workflow_dispatch` validates the release path for the workflow commit. Manual runs upload artifacts to the workflow run only.

The manual `version` input defaults to `latest`, which resolves to the version in `CMakeLists.txt`. A numeric value must match `CMakeLists.txt`.

The manual `build_profile` input accepts `release` or `debug`. Official GitHub Releases always force the release profile. Manual debug runs use `RelWithDebInfo`, keep symbols where practical, skip stripping, add a `-debug` artifact suffix, and upload Windows map files as workflow artifacts only.

## Packaging Guarantees

Release jobs build dependencies from the pinned sources in `packaging/dependencies.env`, verify SHA-256 hashes, run local loopback and hostname smoke tests, check runtime dependency closure, run size audits, generate checksums, and upload final assets.

Official builds use libcurl with c-ares for asynchronous DNS resolution. `aria2-next --version` reports `Async DNS` and `c-ares/<version>` when the linked libcurl exposes that resolver capability.

Linux release binaries must be static ELF executables without unexpected dynamic dependencies. macOS release binaries may link only Apple system libraries and frameworks at runtime. Windows release binaries may link only Windows system DLLs at runtime. Android binaries must not require `libc++_shared.so`.

## Maintainer Release Flow

Finalize and verify the code before starting the release:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

For packaging changes, also run the shell syntax checks listed in [`CONTRIBUTING.md`](CONTRIBUTING.md).

Then bump and release:

```bash
./scripts/bump-version.sh <major.minor.patch>
./scripts/release.sh
```

`release.sh` verifies the local build, CTest, version output, and maintained shell script syntax. It stages changes, commits if needed, creates an annotated tag, pushes the branch, and pushes the tag.

After `release.sh` succeeds, create a GitHub Release with an English title and user-facing release notes. Publishing that release triggers official asset builds.

Release title format:

```text
v<version> - <Concise Release Theme>
```

Release notes should explain user-visible impact, compatibility changes, fixes, security notes, and downloads. Avoid raw commit dumps.

## Failed Releases

Published tags are treated as immutable.

If the GitHub Release has not been created, delete an incorrect tag only after confirming the exact tag.

If a GitHub Release exists and the release has not been consumed, delete the failed GitHub Release and tag, fix the code, rerun verification, and recreate the same tag deliberately.

If a release has been publicly consumed, do not delete or replace it. Publish a new patch version after the maintainer chooses the version.
