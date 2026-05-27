# Contributing to Aria2 Next

Aria2 Next is a maintained aria2-compatible engine. Contributions should preserve the command-line, configuration, session, and JSON-RPC surfaces unless a change explicitly introduces an aria2-next extension.

## Development Setup

Install CMake 3.25 or newer, Ninja, pkg-config, a C99/C++17 compiler, CppUnit, libcurl, OpenSSL, Boost, libtorrent-rasterbar, and zlib.

```bash
git clone https://github.com/AnInsomniacy/aria2-next.git
cd aria2-next
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

The default preset creates `build/default/`. Do not commit build directories, generated CMake files, binaries, logs, dumps, or release artifacts.

## Project Boundaries

Aria2 Next ships the `aria2-next` executable. It does not maintain a public C++ embedding API. Motrix Next consumes it as a sidecar engine through the aria2-compatible CLI and JSON-RPC interfaces.

The supported protocol surface is HTTP, HTTPS, FTP, SFTP, BitTorrent, magnet, ED2K, session files, input files, checksums, and JSON-RPC. BitTorrent is implemented through libtorrent-rasterbar. ED2K is native aria2-next code.

## Code Quality

Keep changes focused. Find the root cause before editing. Avoid drive-by formatting, broad rewrites, and unrelated dependency updates.

Source code follows the existing C and C++ style. The baseline is C99 and C++17. Prefer small helpers with clear ownership over large cross-cutting changes. Use existing engine abstractions before adding new ones.

Build logic belongs in focused modules under `cmake/modules/`. Keep the top-level `CMakeLists.txt` small. CMake is the only supported build system.

## Tests

Use the smallest test that proves the changed behavior, then expand when the change touches shared transfer, storage, RPC, or packaging behavior.

Normal source changes require:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

Packaging or release changes also require:

```bash
bash -n tools/build_test.sh
bash -n scripts/bump-version.sh
bash -n scripts/release.sh
bash -n packaging/scripts/common.sh
bash -n packaging/scripts/mingw-release
bash -n packaging/scripts/android-release
```

For broad CMake option changes, run:

```bash
tools/build_test.sh
```

Do not hide test failures with `|| true`. If a test cannot be run locally, say why in the PR.

## Bug Fixes

Bug fixes should include a regression test when the behavior can be covered locally. Network-dependent behavior should be reduced to a local server, parser test, command-level test, or protocol fixture whenever possible.

When a bug is site-specific, document the server behavior that matters. Examples include ignored HTTP Range requests, invalid `Content-Range`, certificate chain failures, expired signed URLs, proxy environment leakage, torrent metadata timing, or ED2K source availability.

## New Features

Open an issue before implementing new CLI options, JSON-RPC fields, protocol behavior, or release artifact changes. The issue should explain the user problem, compatibility impact, and whether the change is aria2-compatible behavior or an aria2-next extension.

JSON-RPC extensions must be explicit and stable. Do not overload existing aria2 fields with placeholder values. Prefer a small new field with precise semantics.

## Dependency Changes

`packaging/dependencies.env` is the release dependency source of truth. Update it before changing dependency versions in scripts, Dockerfiles, workflows, package notes, or README tables.

Dependency updates must be intentional. Include the upstream version, source archive URL, SHA-256 hash, and affected release path in the PR.

## Commit Messages

Use Conventional Commits.

```text
fix(http): retry transient segmented transfer failures
feat(rpc): expose ED2K visible progress
docs: add release troubleshooting guide
ci(release): add Windows debug artifacts
```

## Pull Requests

Each PR should address one concern. Keep behavioral changes, refactors, dependency updates, and documentation updates separate unless they are required for one fix.

PRs must explain the affected surface, compatibility impact, verification commands, and user-facing release note. The PR template includes the required checklist.

AI-assisted development is allowed when the author reviews, understands, tests, and can explain every change. Fill out the PR disclosure honestly.

## Reporting Security Issues

Do not publish secrets, private cookies, proxy credentials, signed URL tokens, crash dumps with private data, or exploit details in public issues. See `docs/SECURITY.md`.
