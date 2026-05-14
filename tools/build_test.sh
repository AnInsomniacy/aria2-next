#!/bin/sh
set -e

BUILDDIR=${BUILDDIR:-/tmp/aria2buildtest}
GENERATOR=${GENERATOR:-Ninja}
JOBS=${JOBS:-2}
mkdir -p "$BUILDDIR"

build() {
  name=$1
  shift
  dir="$BUILDDIR/$name"
  echo "*** cmake build $name"
  cmake -S . -B "$dir" -G "$GENERATOR" -DCMAKE_BUILD_TYPE=Debug "$@"
  cmake --build "$dir" -j"$JOBS"
  ctest --test-dir "$dir" --output-on-failure
  cp "$dir/aria2c" "$BUILDDIR/aria2c_$name" 2>/dev/null || true
}

case "$1" in
  clear)
    rm -rf "$BUILDDIR"
    ;;
  *)
    build default
    build openssl -DARIA2_WITH_APPLETLS=OFF -DARIA2_WITH_OPENSSL=ON
    build nossl -DARIA2_ENABLE_SSL=OFF
    build nocares -DARIA2_WITH_CARES=OFF
    build expat -DARIA2_WITH_LIBXML2=OFF -DARIA2_WITH_EXPAT=ON
    build noxml -DARIA2_WITH_LIBXML2=OFF -DARIA2_WITH_EXPAT=OFF
    build nozlib -DARIA2_WITH_ZLIB=OFF
    build nosqlite3 -DARIA2_WITH_SQLITE3=OFF
    build nolibssh2 -DARIA2_WITH_LIBSSH2=OFF
    build nobt -DARIA2_ENABLE_BITTORRENT=OFF
    build noml -DARIA2_ENABLE_METALINK=OFF
    build nobt_noml -DARIA2_ENABLE_BITTORRENT=OFF -DARIA2_ENABLE_METALINK=OFF
    build noepoll -DARIA2_ENABLE_EPOLL=OFF
    build noepoll_nocares -DARIA2_ENABLE_EPOLL=OFF -DARIA2_WITH_CARES=OFF
    build libaria2 -DARIA2_ENABLE_LIBARIA2=ON
    ;;
esac
