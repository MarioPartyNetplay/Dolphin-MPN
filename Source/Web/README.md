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
    - **Simulation Mode**: If the WASM binary is missing (current state), the frontend falls back to a JS-based simulator to demonstrate the UI flow (GameCube boot sequence).

## How to Build (Requires Emscripten SDK)

1. **Install Emscripten**: Ensure `emcc` is in your path.
2. **Configure**:
   ```bash
   mkdir build
   cd build
   emcmake cmake .. -DENABLE_NOGUI=ON -DENABLE_HEADLESS=OFF -DEMSCRIPTEN=ON -DUSE_SYSTEM_LIBS=OFF
   ```
   *Note: You may need to disable other backends or dependencies (like Qt) if they are not ported.*
3. **Build**:
   ```bash
   emmake make dolphin-nogui
   ```
4. **Deploy**:
   Copy the generated `dolphin-emu-nogui.wasm` and `dolphin-emu-nogui.js` to `Source/Web/`.
   Serve `Source/Web/` using a web server (e.g., `python3 -m http.server`).

## Features

- **PlatformWeb**: A new C++ platform backend (`Source/Core/DolphinNoGUI/PlatformWeb.cpp`) that interfaces with Emscripten's main loop.
- **Web UI**: A modern dark-themed web interface for the emulator.
- **File Upload**: Native file picker integration to load games into the browser environment.
- **Simulator**: `main.js` includes a fallback simulator for demonstration when the WASM build is unavailable.
