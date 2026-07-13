# AGENTS.md - aria2-next

This file defines repository rules for AI coding agents. Human contributors should start with [README.md](README.md), [packaging/README.md](packaging/README.md), [tools/README.md](tools/README.md), [third_party/README.md](third_party/README.md), and [docs/maintenance/README.md](docs/maintenance/README.md).

> [!IMPORTANT]
> All changes must meet industrial-grade quality. Find the root cause before changing code, keep behavior compatible unless the task explicitly changes it, avoid unrelated churn, and verify the exact build or release path affected by the change.

## Architecture

| Area | Ownership |
| --- | --- |
| Core | C11 and C++17 aria2 command-line client and core implementation |
| Build | CMake 3.25+ with Ninja as the default generator |
| Tests | CTest plus the doctest unit test suite (vendored in third_party/doctest) |
| Packaging | Cross-platform release automation under `packaging/` and `.github/workflows/release.yml` |
| Third-party source | Vendored `third_party/wslay` with local ownership rules |

## Key Paths

| Path | Purpose |
| --- | --- |
| `CMakeLists.txt` | Project declaration and CMake module entry point |
| `CMakePresets.json` | Standard configure, build, and test presets |
| `cmake/modules/` | Build options, probes, dependencies, targets, tests, and summary output |
| `cmake/Sources.cmake` | Core source inventory |
| `cmake/TestSources.cmake` | Test source inventory |
| `src/` | aria2 core, CLI, protocol, disk, RPC, and platform code |
| `src/includes/aria2/` | Public libaria2 headers |
| `tests/` | doctest unit tests and fixtures |
| `docs/` | Manual sources, completion tooling, and maintenance records |
| `packaging/` | Release dependencies, cross-build scripts, and package assets |
| `packaging/dependencies.env` | Release dependency version source |
| `third_party/` | Bundled third-party source |
| `tools/` | Local developer helpers |

## Build Rules

CMake is the only supported build system for aria2-next. Do not restore Autotools files, add another maintained build system, or route release packaging through removed upstream build scripts.

Keep the top-level `CMakeLists.txt` small. New build logic belongs in the focused modules under `cmake/modules/`. Keep generated config headers, package metadata, CMake files, Ninja files, test output, binaries, and archives out of the source tree.

Feature probes must match the source they guard. Prefer `check_symbol_exists`, `check_cxx_symbol_exists`, or small `check_*_source_compiles` snippets with the exact headers needed by the compiled branch. Windows and MinGW probes must include the correct Win32 headers and include order.

## Version Management

`CMakeLists.txt` is the single source of truth for the project version:

```cmake
project(
  aria2
  VERSION 2.0.2
  ...
)
```

`PROJECT_VERSION` feeds generated package metadata and release artifact naming. Scripts that need the version must read it from `CMakeLists.txt`; they must not carry independent version constants.

Use `./scripts/bump-version.sh <major.minor.patch>` to change the CMake project version. The script only accepts plain numeric versions.

Use standard semantic-version bump semantics. A minor release bump from
`2.0.6` is `2.1.0`, not `2.1.6`; a patch bump from `2.0.6` is `2.0.7`.

Release tags use `v{PROJECT_VERSION}`. The tag version and CMake project version must match exactly after removing the leading `v`. Pre-release, beta, RC, channel, build-metadata, or date-based release suffixes are not supported.

Treat published release tags as immutable. If a failed release has not been consumed, delete the failed GitHub Release and tag, fix the commit, then recreate the same release deliberately. If a release has been publicly consumed, stop and report the situation. Do not choose, bump, or publish a replacement version unless the maintainer explicitly specifies the new version.

## Dependency Management

`packaging/dependencies.env` is the authoritative dependency baseline for maintained release packaging. It owns dependency versions, source archive names, download URLs, and SHA-256 hashes. Update it before changing dependency versions in scripts, workflow files, package notes, or README tables.

Do not add automated dependency PR systems, scheduled dependency update workflows, or Dependabot configuration unless explicitly requested. Dependency updates should be intentional and verified through the affected release path.

## Release Process

The release workflow is `.github/workflows/release.yml`. It runs on `release: published` and on manual `workflow_dispatch` validation. Tag pushes do not publish release builds directly.

Maintained release artifacts are standalone executable assets named `aria2-next-<version>-linux-x86_64`, `aria2-next-<version>-linux-aarch64`, `aria2-next-<version>-macos-arm64`, `aria2-next-<version>-macos-x86_64`, `aria2-next-<version>-windows-x86_64.exe`, `aria2-next-<version>-windows-arm64.exe`, `aria2-next-<version>-android-arm64`, and `aria2-next-<version>-checksums.sha256`.

Manual workflow runs are for release-path validation. Official release assets are uploaded only when a GitHub Release is published. Release jobs must check runtime dependency closure before assets are uploaded.

Use `./scripts/release.sh` for releases. This script verifies the local build, stages changes, commits if needed, creates an annotated tag, and pushes the commit and tag. It does not create the GitHub Release, generate release notes, or trigger GitHub Actions manually.

After `release.sh` succeeds, generate an English release title and release notes from the commits included in the release, show them to the maintainer, and wait for approval before creating the GitHub Release. Releases must not be marked as pre-releases. Creating the GitHub Release triggers the official release workflow and asset upload.

Release title format:

```text
v{VERSION} - {Concise Release Theme}
```

Use a concrete theme, not a generic label. Good examples are `v2.0.2 - Release Automation and Packaging Refinements` and `v2.1.0 - Transport Stability Improvements`.

Release notes must be written for users and integrators, not as a raw commit dump. Follow the GitHub Releases and Keep a Changelog convention: use a concrete title, start with a short outcome-focused summary, group notable changes by user impact, mention breaking or security items before general changes, and keep downloads/checksums easy to find. Use these headings only when they contain real content:

```markdown
## Summary

One short paragraph covering the release scope, supported artifacts, and operational impact.

### Added
- New user-visible capabilities, build targets, or supported workflows.

### Changed
- Behavior changes, packaging updates, dependency baseline changes, or workflow changes.

### Fixed
- Bugs, regressions, compatibility issues, and reliability fixes.

### Security
- Security fixes or dependency updates with security impact.

### Breaking Changes
- Any incompatible change, removed behavior, migration requirement, or manual action.

### Downloads
| Platform | Download |
| --- | --- |
| Linux x86_64 | `aria2-next-{VERSION}-linux-x86_64` |
| Linux ARM64 | `aria2-next-{VERSION}-linux-aarch64` |
| macOS Apple Silicon | `aria2-next-{VERSION}-macos-arm64` |
| macOS Intel | `aria2-next-{VERSION}-macos-x86_64` |
| Windows x86_64 | `aria2-next-{VERSION}-windows-x86_64.exe` |
| Windows ARM64 | `aria2-next-{VERSION}-windows-arm64.exe` |
| Android ARM64 | `aria2-next-{VERSION}-android-arm64` |
| Checksums | `aria2-next-{VERSION}-checksums.sha256` |
```

Omit empty sections. Put `Breaking Changes` before `Summary` if users must act before upgrading. Put `Security` before `Summary` if the release contains a security fix. Use present or past tense consistently. Keep bullets specific, outcome-focused, and scoped to user-visible behavior, compatibility, packaging, reproducibility, maintainability, or the release process. Avoid internal-only chores, unchecked claims, future promises, raw commit hashes, and contributor callouts unless the maintainer asks for them. Once assets are uploaded, prefer the concrete `Downloads` list over saying assets will be built later.

Before publishing a GitHub Release, verify locally:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

For packaging changes, also run:

```bash
bash -n tools/build_test.sh
bash -n scripts/bump-version.sh
bash -n scripts/release.sh
bash -n packaging/scripts/common.sh
bash -n packaging/scripts/mingw-release
```

## Failed Release Recovery

If the GitHub Release has not been created, remove the incorrect local and remote tag only after the maintainer confirms the exact tag:

```bash
git tag -d v2.0.2
git push origin --delete v2.0.2
```

If the GitHub Release exists and the release workflow failed before public consumption, delete the failed GitHub Release, delete the local and remote tag, fix the commit, rerun verification, and recreate the same tag deliberately.

If the release has been publicly consumed, do not delete the release, delete the tag, or invent a replacement version. Stop, explain the failure, and wait for the maintainer to specify the exact next version.

## CI and Verification

Use the smallest verification set that covers the changed surface, then expand when changes touch shared build logic, platform probes, or packaging.

When a Python environment is needed, use `source /opt/anaconda3/etc/profile.d/conda.sh && conda activate global`. Non-interactive shells may not have `conda` on `PATH`, so plain `conda activate global` can fail even though the environment exists.

Normal source or CMake changes require:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Broad CMake option changes require the local matrix helper:

```bash
tools/build_test.sh
```

Do not hide new test failures with `|| true`. Existing tolerated diagnostics must stay limited to non-gating inspection commands unless the maintainer explicitly accepts the risk.

## Source Conventions

Follow the existing C and C++ style. Keep C++ at the repository's C++17 baseline unless the project intentionally raises the baseline. Avoid drive-by rewrites and broad formatting churn.

Prefer accurate CMake detection and existing config-header patterns over scattered compatibility macros. Add helpers only when they remove real duplication or contain platform-specific behavior cleanly.

Changes under `third_party/` must preserve third-party ownership. Limit edits there to build integration, security fixes, or compatibility fixes that cannot reasonably wait for upstream.

## Documentation

Keep documentation synchronized with behavior. If a CMake option, dependency baseline, supported platform, artifact name, release command, or directory path changes, update the relevant documentation in the same change.

`docs/maintenance/` is for durable audit records only. Do not commit temporary API payloads, scratch logs, generated reports, or local caches there.

## Git Safety

Work on the current branch unless the user explicitly asks for a new branch. Never revert user changes or unrelated work. Do not use destructive git commands unless the user explicitly requests that exact operation.
