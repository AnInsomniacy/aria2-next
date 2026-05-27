# Windows Package Note

This package contains the aria2-next `aria2-next.exe` binary for Windows. It is statically linked for portable use and keeps the aria2 command-line and RPC interfaces intact.

The official release binary is checked before packaging so it does not require MinGW, LLVM, OpenSSL, or zlib DLLs next to `aria2-next.exe`.

Official Windows releases use OpenSSL with libcurl native CA support, so HTTPS verification uses the Windows certificate store without requiring a bundled CA file or extra DLLs next to `aria2-next.exe`.

Maintained dependency and Windows ARM64 llvm-mingw versions are recorded in `packaging/dependencies.env` in the source tree.

Manual release workflow debug builds are available for diagnosis. They use a `-debug` artifact suffix, keep debug information where practical, and upload a Windows linker map file next to the executable in the workflow run. Debug artifacts are not attached to official GitHub Releases.

When reporting a Windows crash, include the aria2-next version, architecture, command line, exception code, faulting module, fault offset, and whether the binary came from an official release or a debug workflow artifact.

Example use from PowerShell:

```powershell
.\aria2-next.exe --version
.\aria2-next.exe https://example.com/file.iso
```

`--daemon` is not supported on Windows. Use a service manager, scheduler, or parent application when background process management is required.
