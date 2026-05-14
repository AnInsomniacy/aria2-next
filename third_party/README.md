# Third-Party Source

This directory contains source code bundled into aria2-next builds. Files here keep their upstream layout and coding style unless a local build or security fix is required.

## wslay

`third_party/wslay` provides the WebSocket implementation used by aria2 JSON-RPC over WebSocket support. The bundled source is retained because wslay 1.1.1 is still the current upstream release and CMake now builds the required static library directly from its source files.

The bundled copy keeps source files, public headers, license material, and useful upstream notes. Obsolete generated build files and legacy build-system files are intentionally not retained.

aria2-next does not apply aria2 formatting rules to files under `third_party/`. Changes in this directory should be limited to build integration, security fixes, or compatibility fixes that cannot reasonably wait for upstream.

Future work may add system wslay support so distributions can link against a system-provided package. That is intentionally outside the current migration because this pass preserves existing bundled behavior while making the ownership boundary explicit.
