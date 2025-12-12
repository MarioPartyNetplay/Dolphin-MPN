@echo off
setlocal enabledelayedexpansion

echo Starting Dolphin WebAssembly Build (Windows)...

REM Check dependencies
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: cmake is not installed or not in PATH.
    pause
    exit /b 1
)

where git >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: git is not installed or not in PATH.
    pause
    exit /b 1
)

REM 1. Install Emscripten
if not exist "emsdk" (
    echo Cloning Emscripten SDK...
    git clone https://github.com/emscripten-core/emsdk.git
)

if not exist "emsdk\emsdk.bat" (
    echo Error: emsdk\emsdk.bat not found.
    pause
    exit /b 1
)

cd emsdk
if not exist "emsdk_env.bat" (
    echo Installing latest SDK...
    call emsdk.bat install latest
    echo Activating latest SDK...
    call emsdk.bat activate latest
)
echo Setting environment variables...
call emsdk_env.bat
cd ..

REM 2. Configure
echo Configuring Dolphin...
if not exist "build_wasm" mkdir build_wasm
cd build_wasm

REM Note: We use cmake --build so it works with whatever generator is chosen (MinGW, NMake, Ninja, VS)
call emcmake cmake .. ^
  -DENABLE_NOGUI=ON ^
  -DENABLE_QT=OFF ^
  -DENABLE_X11=OFF ^
  -DENABLE_VULKAN=OFF ^
  -DENABLE_EGL=ON ^
  -DENABLE_GENERIC=ON ^
  -DENABLE_HEADLESS=OFF ^
  -DENABLE_TESTS=OFF ^
  -DENABLE_ANALYTICS=OFF ^
  -DENABLE_AUTOUPDATE=OFF ^
  -DENABLE_SDL=OFF ^
  -DUSE_DISCORD_PRESENCE=OFF ^
  -DUSE_UPNP=OFF ^
  -DUSE_RETRO_ACHIEVEMENTS=OFF ^
  -DENABLE_PULSEAUDIO=OFF ^
  -DENABLE_ALSA=OFF ^
  -DENABLE_CUBEB=OFF ^
  -DENABLE_EVDEV=OFF ^
  -DENABLE_LTO=OFF ^
  -DUSE_SYSTEM_LIBS=OFF ^
  -DCMAKE_CXX_FLAGS="-s USE_PTHREADS=1 -s PROXY_TO_PTHREAD=1" ^
  -DCMAKE_EXE_LINKER_FLAGS="-s USE_PTHREADS=1 -s PROXY_TO_PTHREAD=1 -s ALLOW_MEMORY_GROWTH=1"

REM 3. Build
echo Building Dolphin...
call emmake cmake --build . --target dolphin-nogui --parallel

REM 4. Deploy
echo Deploying to Source\Web...
copy /Y dolphin-emu-nogui.js ..\Source\Web\
copy /Y dolphin-emu-nogui.wasm ..\Source\Web\
if exist dolphin-emu-nogui.worker.js copy /Y dolphin-emu-nogui.worker.js ..\Source\Web\

echo Build Complete! Check Source\Web\ for output.
pause
