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
