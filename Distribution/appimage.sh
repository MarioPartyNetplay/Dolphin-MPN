#!/bin/bash

# Environment variables
if [ "$(uname -m)" = "x86_64" ];
then
    export ARCH=x86_64
    echo "CPU architecture detected as x86_64."

elif [ "$(uname -m)" = "aarch64" ];
then
    export ARCH=aarch64
    echo "CPU architecture detected as aarch64."

else
    echo "CPU architecture not supported or detected."
    exit 1
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=$(which qmake6)

# Set Qt environment variables for linuxdeploy-plugin-qt
# If QTDIR is not set, try to infer it from qmake location
if [ -z "$QTDIR" ] && [ -n "$QMAKE" ]; then
    # qmake is typically at <QTDIR>/bin/qmake6, so go up two levels
    QTDIR=$(dirname $(dirname "$QMAKE"))
    export QTDIR
fi

# If QT_BASE_DIR is set, use it as QTDIR
if [ -n "$QT_BASE_DIR" ]; then
    export QTDIR="$QT_BASE_DIR"
fi

# Export Qt plugin path if QTDIR is set
if [ -n "$QTDIR" ]; then
    export QT_PLUGIN_PATH="$QTDIR/plugins"
    echo "Using QTDIR: $QTDIR"
    echo "Using QMAKE: $QMAKE"
fi

# Prepare the AppDir
DESTDIR=./AppDir ninja install

# Debug: Show what was installed
echo "Checking installed files..."
find ./AppDir -type f -name "dolphin-mpn*" 2>/dev/null | head -20
find ./AppDir -type d -name "sys" 2>/dev/null
find ./AppDir -name "*.desktop" 2>/dev/null

mkdir -p ./AppDir/usr/Source/Core
cp -r ./Source/Core/DolphinQt ./AppDir/usr/Source/Core
rm -rf ./AppDir/usr/Source/Core/DolphinQt/CMakeFiles
rm -rf ./AppDir/usr/Source/Core/DolphinQt/dolphin-mpn_autogen
rm -f ./AppDir/usr/Source/Core/DolphinQt/cmake_install.cmake

# Remove optional binaries if they exist
[ -f ./AppDir/usr/bin/dolphin-mpn-nogui ] && rm ./AppDir/usr/bin/dolphin-mpn-nogui
[ -f ./AppDir/usr/bin/dolphin-tool ] && rm ./AppDir/usr/bin/dolphin-tool

# Move sys directory if it exists
if [ -d ./AppDir/usr/share/dolphin-mpn/sys ]; then
    mv ./AppDir/usr/share/dolphin-mpn/sys ./AppDir/usr/bin/Sys
elif [ -d ./AppDir/usr/share/sys ]; then
    mv ./AppDir/usr/share/sys ./AppDir/usr/bin/Sys
else
    echo "WARNING: sys directory not found in expected locations"
    # Try to find it
    SYS_DIR=$(find ./AppDir -type d -name "sys" 2>/dev/null | head -1)
    if [ -n "$SYS_DIR" ]; then
        echo "Found sys directory at: $SYS_DIR"
        mv "$SYS_DIR" ./AppDir/usr/bin/Sys
    else
        echo "ERROR: Could not find sys directory"
        exit 1
    fi
fi

# Clean up share directory
rm -rf ./AppDir/usr/share/dolphin-mpn

# Ensure desktop file exists and fix it
DESKTOP_FILE="./AppDir/usr/share/applications/dolphin-mpn.desktop"
if [ ! -f "$DESKTOP_FILE" ]; then
    # Try to find it
    DESKTOP_FILE=$(find ./AppDir -name "dolphin-mpn.desktop" 2>/dev/null | head -1)
    if [ -z "$DESKTOP_FILE" ]; then
        echo "ERROR: Desktop file not found"
        echo "Contents of ./AppDir/usr/share/applications:"
        ls -la ./AppDir/usr/share/applications/ 2>/dev/null || echo "Directory does not exist"
        exit 1
    fi
    # Copy to expected location if found elsewhere
    if [ "$DESKTOP_FILE" != "./AppDir/usr/share/applications/dolphin-mpn.desktop" ]; then
        mkdir -p ./AppDir/usr/share/applications
        cp "$DESKTOP_FILE" ./AppDir/usr/share/applications/dolphin-mpn.desktop
        DESKTOP_FILE="./AppDir/usr/share/applications/dolphin-mpn.desktop"
    fi
fi

sed -i 's/env QT_QPA_PLATFORM=xcb dolphin-mpn/dolphin-mpn/g' "$DESKTOP_FILE"

# Prepare Tools for building the AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage
wget -N https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage

chmod a+x linuxdeploy-${ARCH}.AppImage
chmod a+x linuxdeploy-plugin-qt-${ARCH}.AppImage
chmod a+x appimagetool-${ARCH}.AppImage

# Build the AppImage
# Find the executable - it might be in different locations depending on CMAKE_INSTALL_PREFIX
EXECUTABLE_PATH=$(find ./AppDir -type f -name "dolphin-mpn" -executable 2>/dev/null | head -1)

if [ -z "$EXECUTABLE_PATH" ]; then
    echo "ERROR: Executable not found in AppDir"
    echo "Searching for dolphin-mpn in AppDir:"
    find ./AppDir -type f -name "*dolphin*" 2>/dev/null
    exit 1
fi

# Ensure it's in the expected location for AppImage
EXPECTED_PATH="./AppDir/usr/bin/dolphin-mpn"
if [ "$EXECUTABLE_PATH" != "$EXPECTED_PATH" ]; then
    echo "Moving executable from $EXECUTABLE_PATH to $EXPECTED_PATH"
    mkdir -p ./AppDir/usr/bin
    cp "$EXECUTABLE_PATH" "$EXPECTED_PATH"
    EXECUTABLE_PATH="$EXPECTED_PATH"
fi

./linuxdeploy-${ARCH}.AppImage \
  --appdir AppDir \
  --executable "$EXECUTABLE_PATH" \
  --plugin qt

# Check if linuxdeploy succeeded
if [ $? -ne 0 ]; then
    echo "ERROR: linuxdeploy failed. Cannot continue."
    exit 1
fi

# Create hook script directory if it doesn't exist and add Qt platform environment variable
mkdir -p ./AppDir/apprun-hooks
if [ -f ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh ]; then
    echo 'env QT_QPA_PLATFORM=xcb' >> ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh
else
    # Create the hook script if it doesn't exist
    echo '#!/bin/bash' > ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh
    echo 'env QT_QPA_PLATFORM=xcb' >> ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh
    chmod +x ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh
fi

# Ensure desktop file is accessible to appimagetool (copy to AppDir root as fallback)
if [ ! -f ./AppDir/dolphin-mpn.desktop ]; then
    if [ -f ./AppDir/usr/share/applications/dolphin-mpn.desktop ]; then
        cp ./AppDir/usr/share/applications/dolphin-mpn.desktop ./AppDir/
    else
        echo "ERROR: Desktop file not found for appimagetool"
        exit 1
    fi
fi

./appimagetool-${ARCH}.AppImage ./AppDir