# Aria2 Next Contributing Guide

Maintained by [@AnInsomniacy](https://github.com/AnInsomniacy). Issues and pull requests are welcome when they are focused, reproducible, and aligned with the engine scope.

## Development Setup

### Prerequisites

- CMake 3.25 or newer
- Ninja
- Make
- Perl
- A C11 and C++20 capable compiler
- No external library packages: all maintained dependencies are vendored under `third_party`

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

aria2-next is a command-line and JSON-RPC download engine. The repository owns the C and C++ engine, protocol integrations, CMake build, tests, release packaging, Docker image definition, and maintenance records.

Rayburst desktop UI issues belong in [rayburst](https://github.com/AnInsomniacy/rayburst). Browser download interception issues belong in [rayburst-connect](https://github.com/AnInsomniacy/rayburst-connect).

## Code Quality

Keep changes focused. Avoid unrelated refactors, formatting churn, or compatibility layers unless the issue explicitly requires removing old behavior.

The maintained build system is CMake. Do not restore Autotools files or add another maintained build system. Keep top-level `CMakeLists.txt` small and put build logic in focused modules under `cmake/modules/`.

Follow the existing C++ style and `.clang-format`. The engine targets C++20. Use existing helpers before adding new abstractions.

Use the [architecture](architecture.md) when placing new behavior. A function
should perform one logical operation; a file should have a coherent reason to
change. Length is a review signal, not a quota: split mixed responsibilities,
but keep related declarations, option tables and protocol cases together when
that makes them easier to inspect. Do not add indirection merely to shorten a
file.

Prefer native library APIs and RAII for resource ownership. Include the headers
that declare the types and functions a translation unit uses. Remove obsolete
wrappers and declarations when their last caller disappears.

Write comments in English. Explain units, ownership, pointer lifetimes, thread
boundaries and the reason for a non-obvious constraint. Document contracts at
their declarations; keep protocol rationale next to the implementation. Avoid
comments that merely repeat the next statement or retain abandoned designs.
These principles follow the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
and [Google's function guidance](https://google.github.io/styleguide/cppguide.html#Write_Short_Functions).

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

Transfer behaviour changes must also pass the affected
`tools/transfer_validation` modules (`python tools/transfer_validation/run.py all`)
and, for HTTP scheduling, `tools/bench/bench.py` compared with a saved baseline.

When the option catalogue changes, regenerate the option reference and the
bash completion:

```bash
python docs/make_options_doc.py build/default/aria2-next > docs/options.md
python docs/completion/make_bash_completion.py build/default/aria2-next > docs/completion/aria2-next
```

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
bash -n packaging/scripts/build-release
bash -n packaging/scripts/check-runtime-deps
bash -n packaging/scripts/verify-runner-tools
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

Release assets are standalone executables plus checksums. A successful official release asset upload dispatches the Docker workflow for the same tag. The Docker workflow also remains available for independent manual publishing from GitHub Release Linux binaries.

## License

By contributing, you agree that your contributions will be licensed under the repository license: [GPLv2](../COPYING), with the preserved OpenSSL linking exception in [docs/licenses/OPENSSL.md](licenses/OPENSSL.md).
