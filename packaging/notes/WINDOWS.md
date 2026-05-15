# Windows Package Note

This package contains the aria2-next `aria2c.exe` binary for Windows. It is statically linked for portable use and keeps the aria2 command-line and RPC interfaces intact.

The official release binary is checked before packaging so it does not require MinGW, LLVM, OpenSSL, zlib, expat, SQLite, c-ares, or libssh2 DLLs next to `aria2c.exe`.

Maintained dependency and Windows ARM64 llvm-mingw versions are recorded in `packaging/dependencies.env` in the source tree.

Example use from PowerShell:

```powershell
.\aria2c.exe --version
.\aria2c.exe https://example.com/file.iso
```

`--daemon` is not supported on Windows. Use a service manager, scheduler, or parent application when background process management is required.
