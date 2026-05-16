# Android ARM64 Package Note

This package contains the aria2-next `aria2-next` binary for Android ARM64. It is a native command-line executable, not an Android application package.

The binary is built with the Android NDK and statically linked release dependencies recorded in `packaging/dependencies.env` in the source tree. The official release binary is checked before packaging so it does not require `libc++_shared.so`.

Example use from an Android shell environment:

```sh
chmod 755 ./aria2-next
./aria2-next --version
./aria2-next https://example.com/file.iso
```

Android certificate and DNS paths differ across shells and devices. Pass `--ca-certificate`, `--async-dns`, or `--async-dns-server` explicitly when the shell environment does not expose usable defaults.
