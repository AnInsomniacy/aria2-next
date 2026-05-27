# Release Guide

This guide describes the maintained aria2-next release process for human maintainers.

## Version Source

`CMakeLists.txt` is the only project version source. Release tags use `v{PROJECT_VERSION}` and must match the CMake version exactly after removing the leading `v`.

Pre-release, beta, RC, channel suffix, build metadata, and date-based release suffixes are not supported for aria2-next.

## Release Artifacts

Official releases publish standalone executable assets:

| Platform | Artifact |
| --- | --- |
| Linux x86_64 | `aria2-next-<version>-linux-x86_64` |
| Linux ARM64 | `aria2-next-<version>-linux-aarch64` |
| macOS Apple Silicon | `aria2-next-<version>-macos-arm64` |
| macOS Intel | `aria2-next-<version>-macos-x86_64` |
| Windows x86_64 | `aria2-next-<version>-windows-x86_64.exe` |
| Windows ARM64 | `aria2-next-<version>-windows-arm64.exe` |
| Checksums | `aria2-next-<version>-checksums.sha256` |

GitHub source archives provide source code and license material.

## Local Release Flow

Finalize code before starting the release.

```bash
./scripts/bump-version.sh 2.2.8
./scripts/release.sh
```

`release.sh` verifies the local build, CTest, version output, and maintained shell script syntax. It stages changes, commits if needed, creates an annotated tag, pushes the branch, and pushes the tag.

After the script succeeds, create a GitHub Release with a clear English title and release notes. Publishing the GitHub Release triggers the official release workflow.

## Workflow Trigger

The release workflow runs on `release: published`. Pushing a tag does not publish binaries.

Manual `workflow_dispatch` is for release-path validation. It uploads artifacts to the workflow run only. Manual runs do not upload official GitHub Release assets.

## Manual Validation Inputs

`version` defaults to `latest`, which means the version currently declared in `CMakeLists.txt`. A numeric value must match `CMakeLists.txt`.

`build_profile` can be `release` or `debug` for manual runs. Official GitHub Releases always force `release`.

The debug profile uses `RelWithDebInfo`, keeps symbols where practical, skips stripping, and gives Windows artifacts a `-debug` suffix plus map-file artifacts. Debug artifacts are for diagnosis and are not uploaded to GitHub Releases.

## Packaging Guarantees

Release jobs build dependencies from the pinned sources in `packaging/dependencies.env`, verify SHA-256 hashes, build static or system-only runtime dependency closures, run local loopback smoke tests, audit size, generate checksums, and upload final assets.

Linux release binaries must be static ELF executables without unexpected dynamic dependencies. macOS release binaries may link only Apple system libraries and frameworks at runtime. Windows release binaries may link only Windows system DLLs at runtime. Android binaries must not require `libc++_shared.so`.

## Failed Release Recovery

If the GitHub Release has not been created, delete an incorrect tag only after confirming the exact tag.

If a GitHub Release exists and the release has not been consumed, delete the failed GitHub Release and tag, fix the code, rerun verification, and recreate the same tag deliberately.

If a release has been publicly consumed, do not delete or replace it. Publish a new patch version after the maintainer chooses the version.

## Release Notes

Use this title format:

```text
v<version> - <Concise Release Theme>
```

Release notes should explain user-visible impact, compatibility changes, fixes, security notes, and downloads. Avoid raw commit dumps.
