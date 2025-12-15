#!/bin/bash
set -e

echo "Starting Dolphin WebAssembly Build..."

cd ..

# Check dependencies
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed."
    exit 1
fi

if ! command -v git &> /dev/null; then
    echo "Error: git is not installed."
    exit 1
fi

# 0. Initialize and update submodules (including emsdk)
echo "Initializing and updating submodules..."
git submodule update --init --recursive

# 1. Install/Update Emscripten
echo "Checking Emscripten SDK..."

if [ ! -d "emsdk" ]; then
    echo "Error: emsdk submodule not found. Please run: git submodule update --init --recursive"
    exit 1
fi

# Ensure emsdk_env.sh exists or try to fix it
if [ ! -f "emsdk/emsdk_env.sh" ]; then
    echo "emsdk_env.sh not found. Attempting to install/activate latest..."
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    cd ..
fi

if [ ! -f "emsdk/emsdk_env.sh" ]; then
    echo "Error: emsdk/emsdk_env.sh still not found after installation attempts."
    echo "Please ensure the emsdk submodule is properly initialized."
    exit 1
fi

echo "Sourcing Emscripten environment..."
source emsdk/emsdk_env.sh

# 2. Configure
echo "Configuring Dolphin..."
mkdir -p build_wasm
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

# 3. Build
echo "Building Dolphin (this may take a while)..."
emmake make dolphin-nogui -j$(nproc)

# 4. Deploy
echo "Deploying to Source/Web..."
mkdir -p ../Source/Web
cp dolphin-emu-nogui.js ../Source/Web/ 2>/dev/null || echo "Warning: dolphin-emu-nogui.js not found"
cp dolphin-emu-nogui.wasm ../Source/Web/ 2>/dev/null || echo "Warning: dolphin-emu-nogui.wasm not found"
if [ -f dolphin-emu-nogui.worker.js ]; then
    cp dolphin-emu-nogui.worker.js ../Source/Web/
fi

echo "Build Complete! Check Source/Web/ for output."
