#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
workdir=${TMPDIR:-/tmp}/aria2-runtime-deps-test.$$
mkdir -p "$workdir/bin"
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

binary="$workdir/static-glibc-marker"
printf 'ELF marker\n__nss_module_get_function\nlibnss_%s.so%s\nGNU C Library\n' > "$binary"
chmod +x "$binary"

cat > "$workdir/bin/readelf" <<'EOF_READELF'
#!/bin/sh
case "$1" in
  -d)
    exit 0
    ;;
  -l)
    exit 0
    ;;
  *)
    exit 2
    ;;
esac
EOF_READELF
chmod +x "$workdir/bin/readelf"

set +e
PATH="$workdir/bin:$PATH" READELF=readelf "$repo_root/packaging/scripts/check-runtime-deps" linux "$binary" >"$workdir/out" 2>"$workdir/err"
status=$?
set -e

if [ "$status" -eq 0 ]; then
  echo "Expected static glibc/NSS marker rejection" >&2
  cat "$workdir/out" >&2
  cat "$workdir/err" >&2
  exit 1
fi

grep -q 'static glibc/NSS marker' "$workdir/err"
