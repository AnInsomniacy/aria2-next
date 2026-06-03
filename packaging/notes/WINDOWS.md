# Windows Package Note

This package contains the aria2-next `aria2-next.exe` binary for Windows. It keeps the aria2 command-line and RPC interfaces intact.

The official release binary is checked before packaging so it does not require MinGW, LLVM, OpenSSL, zlib, expat, SQLite, c-ares, or libssh2 DLLs next to `aria2-next.exe`.

Official Windows releases use WinTLS for HTTPS verification, trust the Windows certificate store, and use WinCNG for libssh2 cryptography. No bundled `cacert.pem` or OpenSSL runtime is required for ordinary HTTPS downloads.

Maintained dependency and Windows ARM64 llvm-mingw versions are recorded in `packaging/dependencies.env` in the source tree.

Example use from PowerShell:

```powershell
.\aria2-next.exe --version
.\aria2-next.exe https://example.com/file.iso
```

`--daemon` is not supported on Windows. Use a service manager, scheduler, or parent application when background process management is required.
