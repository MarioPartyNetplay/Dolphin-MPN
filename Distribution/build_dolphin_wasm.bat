@echo off
setlocal enabledelayedexpansion

echo Starting Dolphin WebAssembly Build (Windows)...

cd ..

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

REM 0. Initialize and update submodules (including emsdk)
echo Initializing and updating submodules...
git submodule update --init --recursive
if %errorlevel% neq 0 (
    echo Warning: Submodule update failed. Continuing anyway...
)

REM 1. Install/Update Emscripten
echo Checking Emscripten SDK...

if not exist "emsdk" (
    echo Error: emsdk submodule not found. Please run: git submodule update --init --recursive
    pause
    exit /b 1
)

if not exist "emsdk\emsdk.bat" (
    echo Error: emsdk\emsdk.bat not found. Please ensure the emsdk submodule is properly initialized.
    pause
    exit /b 1
)

cd emsdk

REM Check if SDK is installed by looking for upstream/emscripten directory
if not exist "upstream\emscripten\emcmake.bat" (
    echo Emscripten SDK not installed. Installing latest SDK...
    call emsdk.bat install latest
    if %errorlevel% neq 0 (
        echo Error: Failed to install Emscripten SDK.
        cd ..
        pause
        exit /b 1
    )
    echo Activating latest SDK...
    call emsdk.bat activate latest
    if %errorlevel% neq 0 (
        echo Error: Failed to activate Emscripten SDK.
        cd ..
        pause
        exit /b 1
    )
)

REM Verify emcmake exists after installation
if not exist "upstream\emscripten\emcmake.bat" (
    echo Error: emcmake.bat not found. SDK installation may be incomplete.
    echo Please run manually: cd emsdk ^&^& emsdk.bat install latest ^&^& emsdk.bat activate latest
    cd ..
    pause
    exit /b 1
)

echo Setting environment variables...
REM Get the EMSDK path (absolute path)
for %%I in (.) do set EMSDK=%%~fI
REM Add emsdk paths to PATH
set PATH=%EMSDK%;%EMSDK%\upstream\emscripten;%EMSDK%\upstream\bin;%PATH%
cd ..

REM 2. Configure
echo Configuring Dolphin...
if not exist "build_wasm" mkdir build_wasm
cd build_wasm

REM Note: We use cmake --build so it works with whatever generator is chosen (MinGW, NMake, Ninja, VS)
REM Use full path to emcmake to ensure it's found
"%EMSDK%\upstream\emscripten\emcmake.bat" cmake .. ^
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
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
  -DCMAKE_CXX_FLAGS="-s USE_PTHREADS=1 -s PROXY_TO_PTHREAD=1" ^
  -DCMAKE_EXE_LINKER_FLAGS="-s USE_PTHREADS=1 -s PROXY_TO_PTHREAD=1 -s ALLOW_MEMORY_GROWTH=1"

REM 3. Build
echo Building Dolphin...
REM Use full path to emmake to ensure it's found
"%EMSDK%\upstream\emscripten\emmake.bat" cmake --build . --target dolphin-nogui --parallel

REM 4. Deploy
echo Deploying to Source\Web...
if not exist "..\Source\Web" mkdir ..\Source\Web
if exist dolphin-emu-nogui.js (
    copy /Y dolphin-emu-nogui.js ..\Source\Web\
) else (
    echo Warning: dolphin-emu-nogui.js not found
)
if exist dolphin-emu-nogui.wasm (
    copy /Y dolphin-emu-nogui.wasm ..\Source\Web\
) else (
    echo Warning: dolphin-emu-nogui.wasm not found
)
if exist dolphin-emu-nogui.worker.js copy /Y dolphin-emu-nogui.worker.js ..\Source\Web\

echo Build Complete! Check Source\Web\ for output.
pause
