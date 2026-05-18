# CMake Migration Progress

## Baseline

The Autotools baseline was captured before introducing the CMake build.

Host summary:

- Build, host, and target were `aarch64-apple-darwin25.5.0`.
- The default compiler pair was `gcc -std=gnu23` and `g++`.
- Enabled default features were SQLite3, SSL, AppleTLS, LibXML2, c-ares, zlib, libssh2, BitTorrent, Metalink, XML-RPC, WebSocket, async DNS, Apple message digest, internal ARC4, and internal bignum.
- Disabled or unavailable features were libuv, WinTLS, GnuTLS, OpenSSL, libnettle, GMP, libgcrypt, Expat, tcmalloc, jemalloc, epoll, libaria2, and static linking.

Baseline verification:

- `./configure` completed successfully and configured bundled `third_party/wslay`.
- `make -j$(sysctl -n hw.ncpu || nproc || echo 2)` completed successfully.
- `make check -j$(sysctl -n hw.ncpu || nproc || echo 2)` completed successfully with `TOTAL: 1`, `PASS: 1`, `FAIL: 0`, and `ERROR: 0`.
- `./src/aria2c --version` reported aria2 1.37.0 with Async DNS, BitTorrent, Firefox3 Cookie, GZip, HTTPS, Message Digest, Metalink, XML-RPC, SFTP, AppleTLS, c-ares, libssh2, zlib, libxml2, and sqlite3.

## 2026-05-14 - Checkpoint 1

Changed:

- Added conditional CMake source inventory files generated from the former `src/Makefile.am` and `tests/Makefile.am` source groups.
- Added a CMake `config.h` template derived from the former Autoconf `config.h.in`.

Verified:

- Inspected the former Autotools option, dependency, platform, source, tests, and bundled `wslay` build surfaces.
- Confirmed the CMake source inventories preserve conditional groups instead of compiling mutually exclusive implementations together.

Remaining:

- Add the root CMake project, presets, feature probes, dependency discovery, targets, tests, and packaging updates.
- Verify CMake default configure, build, and CTest before deleting Autotools files.

## 2026-05-14 - Checkpoint 2

Changed:

- Added the root CMake project and Ninja-based CMake presets.
- Added CMake feature probes for headers, functions, struct members, endian state, TLS backends, XML backends, zlib, SQLite, c-ares, libssh2, fallback allocation support, event polling support, and platform TLS.
- Added target-based CMake targets for bundled `wslay`, the aria2 core library, the `aria2c` executable, and the CppUnit tests executable.
- Added CMake generation for `config.h`, the bundled `wslayver.h`, and `src/libaria2.pc`.
- Ported the default macOS feature selection to CMake with AppleTLS, Apple message digest, c-ares async DNS, libxml2, zlib, SQLite, libssh2, BitTorrent, Metalink, XML-RPC, and WebSocket enabled.

Verified:

- `cmake --preset default` completed successfully.
- `cmake --build --preset default -j$(sysctl -n hw.ncpu || nproc || echo 2)` completed successfully.
- `ctest --preset default` passed with one CTest tests target and zero failures.
- `build/default/aria2c --version` reported aria2 1.37.0 with the same default feature and library surface as the Autotools baseline.

Remaining:

- Run the requested CMake option matrix.
- Update CI, packaging, helper scripts, README, and manual documentation to use CMake only.
- Remove Autotools files after CMake parity and maintained release paths are updated.

## 2026-05-14 - Checkpoint 3

Changed:

- Replaced the maintained build documentation with CMake and Ninja instructions.
- Updated the release workflow to configure aria2-next through CMake for Linux x86_64, Linux ARM64, macOS ARM64, macOS Intel, Windows x86_64, and Windows ARM64 release jobs.
- Updated maintained Docker and release helper paths to build this repository through CMake.
- Kept `packaging/dependencies.env` as the authoritative release dependency version file.
- Removed obsolete release helper scripts that only drove the former Autotools build.

Verified:

- Parsed `.github/workflows/release.yml` with Python YAML and confirmed seven jobs are present.
- Ran `bash -n` over maintained executable helper scripts in `tools/` and `packaging/scripts/`.

Remaining:

- Remove the former Autotools source files and generated build artifacts.
- Re-run fresh CMake builds after the deletion pass.

## 2026-05-14 - Checkpoint 4

Changed:

- Removed the former root Autotools entry files, nested `Makefile.am` files, generated Autotools helpers, libtool helper files, local Autotools outputs, and the unused `m4/` macro directory.
- Removed bundled `wslay` Autotools files and kept it as a small CMake static library target with source, headers, and license/readme material.
- Removed the unsupported gettext catalog workflow, including `po/`, `tools/import-po`, and the unused `lib/gettext.h` compatibility header.
- Removed inactive NLS code paths from maintained source files because the catalog workflow is no longer present.
- Removed retired Travis CI and Raspberry Pi Trusty packaging instead of retaining unsupported legacy paths.
- Updated `.gitignore` for CMake, Ninja, CTest, CPack, compiler products, documentation outputs, tests side effects, release archives, local IDE files, and local issue-analysis exports.

Verified:

- Confirmed no former aria2-next Autotools files remain outside ignored build trees with a filesystem scan for `configure`, `configure.ac`, `Makefile.am`, `Makefile.in`, `aclocal.m4`, `ltmain.sh`, `config.rpath`, `libtool`, `m4`, and `po`.
- Confirmed no stale local Autotools products remain with a filesystem scan for `.deps`, `.libs`, `*.lo`, `*.la`, `config.status`, `config.log`, `stamp-h1`, and `libwslay.pc`.
- Confirmed maintained paths no longer reference aria2-next Autotools, retired NLS, gettext, or Travis CI terms. Remaining `sqlite-autoconf` matches the upstream SQLite source archive name and is not part of this repository build system.

Remaining:

- Run fresh default CMake verification and the requested option matrix after the final cleanup.

## 2026-05-14 - Checkpoint 5

Changed:

- Rebuilt the default CMake tree from scratch after Autotools and NLS cleanup.
- Verified the default install layout without libaria2 enabled.
- Verified the libaria2 install layout with `ARIA2_ENABLE_LIBARIA2=ON`.

Verified:

- `cmake --preset default` completed successfully from a clean `build/default` tree.
- `cmake --build --preset default -j$(sysctl -n hw.ncpu || nproc || echo 2)` completed successfully.
- `ctest --preset default --output-on-failure` passed with one CTest tests target and zero failures.
- `build/default/aria2c --version` reported aria2 1.37.0 with Async DNS, BitTorrent, Firefox3 Cookie, GZip, HTTPS, Message Digest, Metalink, XML-RPC, SFTP, zlib, libxml2, sqlite3, AppleTLS, c-ares, and libssh2.
- `cmake --install build/default --prefix /tmp/aria2-next-cmake-install-default` installed only `bin/aria2c`.
- A fresh `ARIA2_ENABLE_LIBARIA2=ON` build passed configure, build, and CTest, then installed `bin/aria2c`, `include/aria2/aria2.h`, `lib/libaria2.a`, and `lib/pkgconfig/libaria2.pc`.

Remaining:

- Run the fragile option matrix requested for the one-shot migration.

## 2026-05-14 - Checkpoint 6

Changed:

- Exercised the historically fragile CMake option combinations in independent fresh build trees.

Verified:

- `build/no-bt-no-metalink` passed configure, build, and CTest with BitTorrent and Metalink disabled.
- `build/no-websocket` passed configure, build, and CTest with WebSocket disabled.
- `build/no-cares` passed configure, build, and CTest with c-ares disabled; `aria2c --version` no longer reported Async DNS or c-ares.
- `build/appletls` passed configure, build, and CTest with AppleTLS selected on macOS.
- `build/openssl` passed configure, build, and CTest with AppleTLS disabled and OpenSSL selected; `aria2c --version` reported OpenSSL instead of AppleTLS.
- `build/static` passed configure, build, and CTest with `ARIA2_ENABLE_STATIC=ON`.

Remaining:

- Re-run final static validation and record host release limitations.

## 2026-05-14 - Final Validation

Verified:

- Maintained shell scripts passed `bash -n`.
- `.github/workflows/release.yml` parsed successfully and exposed seven jobs: Linux x86_64, Linux ARM64, macOS ARM64, macOS Intel, Windows x86_64, Windows ARM64, and release.
- Docker CLI is installed locally, but the Docker daemon is not running: `docker info` cannot connect to `/Users/sekiro/.docker/run/docker.sock`. Full Linux, Windows, Android, and GitHub Actions runner parity cannot be proven on this macOS ARM64 host without executing the actual CI runner matrix and cross-platform release environments. Repository-side validation was completed through CMake release-path updates, YAML parsing, shell syntax checks, local default builds, local libaria2 install verification, and local option-matrix builds.
- AppleClang still emits existing platform deprecation warnings for SecureTransport and CommonCrypto MD5 usage. These warnings predate the build-system migration and do not block the verified CMake build or CTest run.

Remaining:

- No repository-side CMake migration blocker remains.
