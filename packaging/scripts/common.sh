# aria2-next helper functions

get_version() {
  VERSION=$(sed -n 's/^  VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -n 1)
  echo "Version: $VERSION"
}

verify_sha256() {
  archive=$1
  expected=$2

  if [ -z "$archive" ] || [ -z "$expected" ]; then
    echo "verify_sha256 requires archive and expected hash" >&2
    exit 1
  fi

  if command -v sha256sum >/dev/null 2>&1; then
    printf '%s  %s\n' "$expected" "$archive" | sha256sum -c -
    return
  fi

  if command -v shasum >/dev/null 2>&1; then
    actual=$(shasum -a 256 "$archive" | awk '{print $1}')
    if [ "$actual" = "$expected" ]; then
      echo "$archive: OK"
      return
    fi

    echo "$archive: FAILED" >&2
    echo "expected: $expected" >&2
    echo "actual:   $actual" >&2
    exit 1
  fi

  echo "No SHA-256 checksum utility found" >&2
  exit 1
}

curl_fetch() {
  url=$1
  archive=$2
  expected=$3

  if [ -z "$url" ] || [ -z "$archive" ] || [ -z "$expected" ]; then
    echo "curl_fetch requires URL, archive, and expected hash" >&2
    exit 1
  fi

  curl --fail --show-error -L --retry 5 --connect-timeout 15 -O "$url"
  verify_sha256 "$archive" "$expected"
}

aria2_install_dir() {
  dir=$1

  if mkdir -p "$dir" 2>/dev/null; then
    return
  fi

  sudo mkdir -p "$dir"
}

aria2_copy_tree() {
  src=$1
  dest=$2

  if cp -R "$src" "$dest" 2>/dev/null; then
    return
  fi

  sudo cp -R "$src" "$dest"
}

aria2_cmake_install() {
  build_dir=$1

  if cmake --install "$build_dir" 2>/dev/null; then
    return
  fi

  sudo cmake --install "$build_dir"
}

install_boost_headers() {
  if [ -z "$PREFIX" ]; then
    echo "install_boost_headers requires PREFIX" >&2
    exit 1
  fi

  tar xf "$BOOST_ARCHIVE"
  aria2_install_dir "$PREFIX/include"
  aria2_copy_tree "boost_${BOOST_VERSION_UNDERSCORE}/boost" "$PREFIX/include/"
}

build_libtorrent_rasterbar() {
  if [ -z "$PREFIX" ]; then
    echo "build_libtorrent_rasterbar requires PREFIX" >&2
    exit 1
  fi

  tar xf "$LIBTORRENT_ARCHIVE"
  cmake -S "libtorrent-rasterbar-$LIBTORRENT_VERSION" \
    -B build/libtorrent-rasterbar-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DBUILD_SHARED_LIBS=OFF \
    -Dbuild_tests=OFF \
    -Dbuild_examples=OFF \
    -Dbuild_tools=OFF \
    -Dpython-bindings=OFF \
    -Dpython-egg-info=OFF \
    -Dgnutls=OFF \
    -Dencryption=ON \
    -Ddht=ON \
    -DOPENSSL_USE_STATIC_LIBS=ON \
    -DOPENSSL_ROOT_DIR="$PREFIX" \
    -DOPENSSL_INCLUDE_DIR="$PREFIX/include" \
    -DOPENSSL_SSL_LIBRARY="$PREFIX/lib/libssl.a" \
    -DOPENSSL_CRYPTO_LIBRARY="$PREFIX/lib/libcrypto.a" \
    -DBoost_NO_BOOST_CMAKE=ON \
    -DBoost_INCLUDE_DIR="$PREFIX/include" \
    -DCMAKE_C_FLAGS="${RELEASE_CFLAGS:-}" \
    -DCMAKE_CXX_FLAGS="${RELEASE_CXXFLAGS:-}" \
    -DCMAKE_EXE_LINKER_FLAGS="${RELEASE_LDFLAGS:-}" \
    "$@"
  cmake --build build/libtorrent-rasterbar-release -j"${ARIA2_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}"
  aria2_cmake_install build/libtorrent-rasterbar-release
}
