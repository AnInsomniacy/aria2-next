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

build_libssh2_for_curl() {
  if [ -z "$PREFIX" ]; then
    echo "build_libssh2_for_curl requires PREFIX" >&2
    exit 1
  fi

  tar xf "$LIBSSH2_ARCHIVE"
  cmake -S "libssh2-$LIBSSH2_VERSION" \
    -B build/libssh2-for-curl-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCMAKE_MODULE_LINKER_FLAGS="${RELEASE_LDFLAGS:-}" \
    -DCMAKE_SHARED_LINKER_FLAGS="${RELEASE_LDFLAGS:-}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DCRYPTO_BACKEND=OpenSSL \
    -DENABLE_ZLIB_COMPRESSION=ON \
    -DOPENSSL_USE_STATIC_LIBS=ON \
    -DOPENSSL_ROOT_DIR="$PREFIX" \
    -DOPENSSL_INCLUDE_DIR="$PREFIX/include" \
    -DOPENSSL_SSL_LIBRARY="$PREFIX/lib/libssl.a" \
    -DOPENSSL_CRYPTO_LIBRARY="$PREFIX/lib/libcrypto.a" \
    -DZLIB_USE_STATIC_LIBS=ON \
    -DZLIB_ROOT="$PREFIX" \
    -DZLIB_INCLUDE_DIR="$PREFIX/include" \
    -DZLIB_LIBRARY="$PREFIX/lib/libz.a" \
    -DCMAKE_C_FLAGS="${RELEASE_CFLAGS:-}" \
    -DCMAKE_EXE_LINKER_FLAGS="${RELEASE_LDFLAGS:-}" \
    "$@"
  cmake --build build/libssh2-for-curl-release -j"${ARIA2_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}"
  aria2_cmake_install build/libssh2-for-curl-release
}

build_cares() {
  if [ -z "$PREFIX" ]; then
    echo "build_cares requires PREFIX" >&2
    exit 1
  fi

  tar xf "$CARES_ARCHIVE"
  cmake -S "c-ares-$CARES_VERSION" \
    -B build/c-ares-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCARES_STATIC=ON \
    -DCARES_SHARED=OFF \
    -DCARES_STATIC_PIC=ON \
    -DCARES_BUILD_TESTS=OFF \
    -DCARES_BUILD_CONTAINER_TESTS=OFF \
    -DCARES_BUILD_TOOLS=OFF \
    -DCARES_INSTALL=ON \
    -DCMAKE_C_FLAGS="${RELEASE_CFLAGS:-}" \
    -DCMAKE_EXE_LINKER_FLAGS="${RELEASE_LDFLAGS:-}" \
    "$@"
  cmake --build build/c-ares-release -j"${ARIA2_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}"
  aria2_cmake_install build/c-ares-release
}

build_curl() {
  if [ -z "$PREFIX" ]; then
    echo "build_curl requires PREFIX" >&2
    exit 1
  fi

  curl_target_system=${CMAKE_SYSTEM_NAME:-}
  for arg in "$@"; do
    case "$arg" in
      -DCMAKE_SYSTEM_NAME=*)
        curl_target_system=${arg#-DCMAKE_SYSTEM_NAME=}
        ;;
    esac
  done
  if [ -z "$curl_target_system" ]; then
    curl_target_system=$(uname -s)
  fi

  curl_ca_options=
  curl_resolver_options="-DENABLE_ARES=ON -DENABLE_THREADED_RESOLVER=OFF -DCARES_USE_STATIC_LIBS=ON -DCARES_INCLUDE_DIR=$PREFIX/include -DCARES_LIBRARY=$PREFIX/lib/libcares.a"
  case "$curl_target_system" in
    Windows)
      curl_ca_options="-DCURL_CA_NATIVE=ON -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none"
      ;;
    Darwin)
      curl_ca_options="-DUSE_APPLE_SECTRUST=ON -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none"
      ;;
    Linux)
      curl_ca_options="-DCURL_CA_BUNDLE=auto -DCURL_CA_PATH=auto -DCURL_CA_FALLBACK=ON"
      ;;
    Android)
      curl_ca_options="-DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none"
      ;;
  esac

  tar xf "$CURL_ARCHIVE"
  # shellcheck disable=SC2086
  cmake -S "curl-$CURL_VERSION" \
    -B build/curl-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCMAKE_MODULE_LINKER_FLAGS="${RELEASE_LDFLAGS:-}" \
    -DCMAKE_SHARED_LINKER_FLAGS="${RELEASE_LDFLAGS:-}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_CURL_EXE=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_LIBCURL_DOCS=OFF \
    -DBUILD_MISC_DOCS=OFF \
    -DCURL_USE_PKGCONFIG=OFF \
    -DCURL_USE_OPENSSL=ON \
    $curl_resolver_options \
    -DOPENSSL_USE_STATIC_LIBS=ON \
    -DOPENSSL_ROOT_DIR="$PREFIX" \
    -DOPENSSL_INCLUDE_DIR="$PREFIX/include" \
    -DOPENSSL_SSL_LIBRARY="$PREFIX/lib/libssl.a" \
    -DOPENSSL_CRYPTO_LIBRARY="$PREFIX/lib/libcrypto.a" \
    -DCURL_ZLIB=ON \
    -DZLIB_USE_STATIC_LIBS=ON \
    -DZLIB_ROOT="$PREFIX" \
    -DZLIB_INCLUDE_DIR="$PREFIX/include" \
    -DZLIB_LIBRARY="$PREFIX/lib/libz.a" \
    -DCURL_USE_LIBSSH2=ON \
    -DLibssh2_ROOT="$PREFIX" \
    -DUSE_NGHTTP2=OFF \
    -DUSE_NGTCP2=OFF \
    -DUSE_NGHTTP3=OFF \
    -DUSE_QUICHE=OFF \
    -DUSE_LIBIDN2=OFF \
    -DCURL_USE_LIBPSL=OFF \
    -DCURL_BROTLI=OFF \
    -DCURL_ZSTD=OFF \
    -DCURL_ENABLE_NTLM=OFF \
    -DCURL_ENABLE_SMB=OFF \
    -DCURL_DISABLE_AWS=ON \
    -DCURL_DISABLE_DOH=ON \
    -DCURL_DISABLE_FILE=ON \
    -DCURL_DISABLE_IPFS=ON \
    -DCURL_DISABLE_LDAP=ON \
    -DCURL_DISABLE_LDAPS=ON \
    -DCURL_DISABLE_DICT=ON \
    -DCURL_DISABLE_GOPHER=ON \
    -DCURL_DISABLE_IMAP=ON \
    -DCURL_DISABLE_MQTT=ON \
    -DCURL_DISABLE_POP3=ON \
    -DCURL_DISABLE_RTSP=ON \
    -DCURL_DISABLE_SMTP=ON \
    -DCURL_DISABLE_TELNET=ON \
    -DCURL_DISABLE_TFTP=ON \
    -DCURL_DISABLE_WEBSOCKETS=ON \
    -DCMAKE_C_FLAGS="${RELEASE_CFLAGS:-}" \
    -DCMAKE_EXE_LINKER_FLAGS="${RELEASE_LDFLAGS:-}" \
    $curl_ca_options \
    "$@"
  cmake --build build/curl-release -j"${ARIA2_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}"
  aria2_cmake_install build/curl-release
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
