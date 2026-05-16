#!/usr/bin/env bash
# Verify, commit, tag, and push a release. GitHub Release notes are authored by the agent.
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  ./scripts/release.sh

This script stops after pushing the commit and annotated tag.
Create the GitHub Release only after reviewing the release title and notes.
USAGE
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CMAKE_LISTS="$PROJECT_ROOT/CMakeLists.txt"

if [ "$#" -eq 1 ] && { [ "$1" = "-h" ] || [ "$1" = "--help" ]; }; then
    usage
    exit 0
fi

if [ "$#" -ne 0 ]; then
  usage >&2
  exit 1
fi

cd "$PROJECT_ROOT"

VERSION=$(sed -n 's/^  VERSION \([0-9][0-9.]*\).*/\1/p' "$CMAKE_LISTS" | head -n 1)

if [ -z "$VERSION" ]; then
  echo "Unable to read project version from $CMAKE_LISTS" >&2
  exit 1
fi

if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid CMake project version: $VERSION" >&2
  echo "Expected major.minor.patch." >&2
  exit 1
fi

TAG="v$VERSION"

if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
  echo "Local tag already exists: $TAG" >&2
  exit 1
fi

if git ls-remote --exit-code --tags origin "refs/tags/$TAG" >/dev/null 2>&1; then
  echo "Remote tag already exists: $TAG" >&2
  exit 1
fi

echo "Preparing $TAG"

cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version

bash -n tools/build_test.sh
bash -n packaging/scripts/common.sh
bash -n packaging/scripts/mingw-release
bash -n packaging/scripts/android-release
bash -n scripts/bump-version.sh
bash -n scripts/release.sh

git add -A

if git diff --cached --quiet; then
  echo "No changes to commit. Tagging current HEAD."
else
  git commit -m "release: $TAG"
fi

git tag -a "$TAG" -m "$TAG"
git push
git push origin "$TAG"

echo "Published git tag: $TAG"
echo "Next: review the release title and notes before creating the GitHub Release."
