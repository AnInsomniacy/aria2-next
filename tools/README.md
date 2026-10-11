# Tools

This directory contains local developer helpers.

| Script | Purpose |
| --- | --- |
| `build_test.sh` | Runs the local CMake build matrix (default, -Werror, size-optimized, LTO) |
| `bench/` | HTTP throughput and recovery benchmark against a local adversarial server |
| `transfer_validation/run` | Runs manually selected transfer validation modules |
| `../scripts/bump-version.sh` | Updates the CMake project version |
| `../scripts/release.sh` | Verifies, commits, tags, and pushes a release |

Release packaging helpers live under `packaging/scripts/`.

The build matrix preserves build output under `build/option-matrix` and resets
each selected CMake cache with `--fresh`. Run all configurations with
`bash tools/build_test.sh`, or pass names such as `werror lto` to rerun only
those configurations. `BUILDDIR`, `GENERATOR` and `JOBS` customize local execution.
Set `ARIA2_DEPENDENCY_ROOT` to reuse dependencies built with the same toolchain;
otherwise the helper builds them once. Native CMake environment variables,
including `CMAKE_TOOLCHAIN_FILE`, `CC`, `CXX` and `CXXFLAGS`, apply normally.
