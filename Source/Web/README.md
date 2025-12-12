# Dolphin Web Port

This directory contains the web frontend for the Dolphin Emulator.

## Overview

This project ports the Dolphin Emulator to WebAssembly using Emscripten.
It includes a C++ backend (`PlatformWeb`), a web frontend (`index.html`, `main.js`), and build scripts.

### Current Status

- **Frontend**: Functional UI with file picker and VFS integration.
- **Runtime**:
  - **Stub Mode** (Default): A JavaScript-based stub (`dolphin-emu-nogui.js`) simulates the WASM runtime to verify the integration pipeline. It receives the uploaded file and prints confirmation logs.
  - **Real Mode**: requires compiling the full C++ codebase.
- **Build System**: `build_dolphin_wasm.sh` script provided to automate the compilation of the real emulator.

## How to Run (Stub Mode)

1. Open `Source/Web/index.html` in a browser.
   *(Note: Browsers may block file access if opened directly. Use a local server: `python3 -m http.server` inside `Source/Web`)*
2. Select an ISO/GCM file.
3. Click "Boot Game".
4. Observe the System Log showing the file being loaded into the VFS and passed to the "core".

## How to Build Real Emulator

**Note:** Compiling Dolphin requires a powerful machine (16GB+ RAM recommended) and Emscripten SDK.

1. Run the build script:
   ```bash
   ./build_dolphin_wasm.sh
   ```
   This script will:
   - Install Emscripten SDK.
   - Configure CMake with Pthreads and WebAssembly flags.
   - Compile the project (this takes a long time).
   - Copy the resulting `.wasm` and `.js` files to `Source/Web`.

2. Serve with COOP/COEP headers (required for Pthreads):
   Create `coop_server.py`:
   ```python
   from http.server import HTTPServer, SimpleHTTPRequestHandler
   class COOPHandler(SimpleHTTPRequestHandler):
       def end_headers(self):
           self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
           self.send_header("Cross-Origin-Opener-Policy", "same-origin")
           super().end_headers()
   HTTPServer(('', 8000), COOPHandler).serve_forever()
   ```
   Run: `python3 coop_server.py` inside `Source/Web/`.
