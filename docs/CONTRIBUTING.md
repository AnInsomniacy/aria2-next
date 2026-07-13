# Aria2 Next Contributing Guide

Maintained by [@AnInsomniacy](https://github.com/AnInsomniacy). Issues and pull requests are welcome when they are focused, reproducible, and aligned with the engine scope.

## Development Setup

### Prerequisites

- CMake 3.25 or newer
- Ninja
- A C99 and C++11 capable compiler
- No extra test dependency: the doctest framework is vendored in third_party/doctest
- OpenSSL, zlib, SQLite, Expat or libxml2, c-ares, and libssh2 development packages when building the default feature set

### Getting Started

```bash
git clone https://github.com/AnInsomniacy/aria2-next.git
cd aria2-next
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

## Scope

aria2-next is an aria2-compatible command-line engine and library. The repository owns the C and C++ engine, protocol implementations, CMake build, tests, release packaging, Docker image definition, and maintenance records.

Motrix Next desktop UI issues belong in [motrix-next](https://github.com/AnInsomniacy/motrix-next). Browser download interception issues belong in [motrix-next-extension](https://github.com/AnInsomniacy/motrix-next-extension).

## Code Quality

Keep changes focused. Avoid unrelated refactors, formatting churn, or compatibility layers unless the issue explicitly requires removing old behavior.

The maintained build system is CMake. Do not restore Autotools files or add another maintained build system. Keep top-level `CMakeLists.txt` small and put build logic in focused modules under `cmake/modules/`.

Follow the existing C and C++ style. Keep C++ at the repository's C++11 baseline unless the project intentionally raises it. Use existing helpers and config-header patterns before adding new abstractions.

Files under `third_party/` keep their upstream ownership. Edit them only for build integration, security fixes, or compatibility fixes that cannot reasonably wait for upstream.

## Testing

Use the smallest verification set that covers the changed surface. Normal source and CMake changes should pass:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

Add focused tests for risky behavior: protocol parsing, state machines, data integrity, cross-platform path handling, RPC contracts, and regressions that could corrupt downloads or break compatibility.

Do not add noisy tests for simple documentation, template, formatting, or mechanical cleanup changes. Do not add tests that depend on real external URLs.

Broad CMake option changes should run:

```bash
tools/build_test.sh
```

Packaging changes should also validate maintained shell scripts:

```bash
bash -n tools/build_test.sh
bash -n scripts/bump-version.sh
bash -n scripts/release.sh
bash -n packaging/scripts/common.sh
bash -n packaging/scripts/check-runtime-deps
bash -n packaging/scripts/verify-runner-tools
bash -n packaging/scripts/mingw-release
```

## Dependencies

`packaging/dependencies.env` is the authoritative dependency baseline for release packaging. Update it before changing dependency versions in scripts, workflows, package notes, or README tables.

Dependency updates should be intentional and verified through the affected release path. Do not add automated dependency update workflows unless maintainers explicitly request them.

## Pull Requests

Use Conventional Commits for PR titles:

```text
fix(http): handle stalled async DNS resolution on Windows
feat(ed2k): add direct callback request handling
docs: clarify Docker runtime volumes
ci(release): tighten Android dependency checks
```

Open or reference an issue for bug fixes and new features. Refactors must stay behavior-neutral. Keep each PR focused on one concern.

For packaging changes, state which release path is affected: Linux, macOS, Windows, Android, Docker, checksums, dependency baseline, runtime dependency closure, or release documentation.

## AI-Assisted Development

AI tools are allowed, but contributors must review, understand, test, and be able to explain every submitted change.

Disclose AI usage in the PR template. Use the exact model name when known.

## Release Process

`CMakeLists.txt` is the project version source of truth. Use `scripts/bump-version.sh` to change it. Use `scripts/release.sh` for releases. GitHub Release notes are reviewed separately before publishing.

Release assets are standalone executables plus checksums. Docker images are published by the separate manual Docker workflow from the latest GitHub Release Linux binaries.

## License

By contributing, you agree that your contributions will be licensed under the repository license: [GPLv2](../COPYING), with the preserved OpenSSL linking exception in [docs/licenses/OPENSSL.md](licenses/OPENSSL.md).
