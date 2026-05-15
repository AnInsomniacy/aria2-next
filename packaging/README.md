# Packaging

This directory owns release packaging, cross-compilation helpers, Docker build contexts, platform package resources, and release dependency metadata.

`dependencies.env` is the authoritative dependency source for maintained release automation. It records versions, archive names, URLs, and SHA-256 hashes for downloaded release inputs.

## Layout

| Path | Purpose |
| --- | --- |
| `notes/` | Platform notes copied into binary packages |
| `docker/` | Dockerfiles for reproducible cross-platform build images |
| `macos/` | macOS package resources |
| `scripts/` | Release packaging helpers |
| `dependencies.env` | Maintained dependency baseline and source archive hashes |

Supported packaging paths build this repository checkout through CMake. Third-party dependencies may use their own upstream build systems while they are being built as release dependencies.

Binary packages should include `README.md`, license files, and the relevant platform note from `notes/`. Release history belongs in git tags and GitHub Releases.

Release jobs must verify runtime dependency closure before packaging. Use `scripts/check-runtime-deps` on the final stripped binary so compiler runtimes and third-party shared libraries cannot leak into portable artifacts.
