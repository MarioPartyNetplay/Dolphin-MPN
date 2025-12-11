# Dolphin Web Port (Proof of Concept)

This directory contains the web frontend for the Dolphin Emulator.

## Overview

This is a functional Proof of Concept (PoC) for running Dolphin in a web browser using WebAssembly.
It demonstrates the directory structure, build system changes, and frontend code required to integrate Emscripten.

### Status

- **Build System**: CMakeLists.txt and Platform code have been updated to support `EMSCRIPTEN` builds.
- **Frontend**: A HTML/JS/CSS frontend is provided (`index.html`, `style.css`, `main.js`).
- **File System**: The frontend supports selecting an ISO/GCM/RVZ file, which is then uploaded to the Emscripten Virtual File System (VFS).
- **Emulation**:
    - **Real Mode**: If `dolphin-emu-nogui.wasm` is compiled and present, the frontend will boot the emulator with the selected ISO.
    - **Simulation Mode**: If the WASM binary is missing, the frontend falls back to a JS-based simulator.

## Build Instructions

### Prerequisites
- Emscripten SDK (latest)
- CMake
- Ninja (optional, for faster builds)

### Compilation

1. **Install Emscripten**:
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   cd ..
   ```

2. **Configure**:
   Dolphin requires Pthreads support for its core loop.
   ```bash
   mkdir build_wasm
   cd build_wasm
   emcmake cmake .. \
     -DENABLE_NOGUI=ON \
     -DENABLE_QT=OFF \
     -DENABLE_X11=OFF \
     -DENABLE_VULKAN=OFF \
     -DENABLE_EGL=ON \
     -DENABLE_GENERIC=ON \
     -DENABLE_HEADLESS=OFF \
     -DENABLE_TESTS=OFF \
     -DENABLE_ANALYTICS=OFF \
     -DENABLE_AUTOUPDATE=OFF \
     -DENABLE_SDL=OFF \
     -DUSE_DISCORD_PRESENCE=OFF \
     -DUSE_UPNP=OFF \
     -DUSE_RETRO_ACHIEVEMENTS=OFF \
     -DENABLE_PULSEAUDIO=OFF \
     -DENABLE_ALSA=OFF \
     -DENABLE_CUBEB=OFF \
     -DENABLE_EVDEV=OFF \
     -DENABLE_LTO=OFF \
     -DUSE_SYSTEM_LIBS=OFF \
     -DCMAKE_CXX_FLAGS="-s USE_PTHREADS=1 -s PROXY_TO_PTHREAD=1" \
     -DCMAKE_EXE_LINKER_FLAGS="-s USE_PTHREADS=1 -s PROXY_TO_PTHREAD=1 -s ALLOW_MEMORY_GROWTH=1"
   ```

3. **Build**:
   ```bash
   emmake make dolphin-nogui -j$(nproc)
   ```

4. **Deploy**:
   Copy `dolphin-emu-nogui.wasm`, `dolphin-emu-nogui.js`, and `dolphin-emu-nogui.worker.js` to `Source/Web/`.

5. **Serve**:
   You must serve the files with COOP/COEP headers for SharedArrayBuffer support (required for Pthreads).

   Example `coop_server.py`:
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

## Features implemented

- **PlatformWeb**: C++ backend (`Source/Core/DolphinNoGUI/PlatformWeb.cpp`) interfacing with Emscripten.
- **GLContextEGL**: Updated to support `WindowSystemType::Web`.
- **Frontend**: `main.js` with VFS integration and "Mock Mode".
