# Legacy Packaging

This directory contains historical packaging assets that are no longer part of
the maintained release pipeline.

`Dockerfile.raspberrypi` is kept only as a reference for the old Raspberry Pi
cross-build flow. It is not maintained because it is based on Ubuntu Trusty,
downloads obsolete dependency releases, and does not match the current
aria2-next dependency baseline.

Maintained release packaging lives in:

- `packaging/docker/`
- `packaging/scripts/`
- `.github/workflows/release.yml`

Do not use files in this directory for new releases without first replacing the
base image, dependency versions, and build assumptions.
