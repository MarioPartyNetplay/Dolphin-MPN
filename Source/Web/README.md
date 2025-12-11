# Dolphin Web Port (Proof of Concept)

This directory contains the web frontend for the Dolphin Emulator.

## Overview

This is a Proof of Concept (PoC) for running Dolphin in a web browser using WebAssembly.
It demonstrates the directory structure and code changes required to integrate the Emscripten toolchain into the Dolphin build system.

### Status

- **Build System**: CMakeLists.txt and Platform code have been updated to support `EMSCRIPTEN` builds.
- **Frontend**: A HTML/JS/CSS frontend is provided (`index.html`, `style.css`, `main.js`).
- **Emulation**: Since compiling the full Dolphin C++ codebase to Wasm requires a complex environment (Emscripten SDK), the `main.js` currently includes a **simulator** that mimics the emulator's behavior (visuals, boot sequence, FPS counter) to demonstrate the web interface functionality.

## How to Build (Hypothetical)

If you have the Emscripten SDK installed and configured:

```bash
mkdir build
cd build
emcmake cmake .. -DENABLE_NOGUI=ON -DENABLE_HEADLESS=OFF -DEMSCRIPTEN=ON
emmake make
```

This would produce a `.wasm` and `.js` file that `index.html` would load.

## Features

- **PlatformWeb**: A new C++ platform backend (`Source/Core/DolphinNoGUI/PlatformWeb.cpp`) that interfaces with Emscripten's main loop.
- **Web UI**: A modern dark-themed web interface for the emulator.
- **Simulator**: `main.js` provides a "1 FPS emulation" proof of concept by simulating a GameCube boot sequence and render loop using HTML5 Canvas.
