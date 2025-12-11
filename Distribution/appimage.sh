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
mkdir -p ./AppDir/usr/Source/Core
cp -r ./Source/Core/DolphinQt ./AppDir/usr/Source/Core
rm -rf ./AppDir/usr/Source/Core/DolphinQt/CMakeFiles
rm -rf ./AppDir/usr/Source/Core/DolphinQt/dolphin-mpn_autogen
rm ./AppDir/usr/Source/Core/DolphinQt/cmake_install.cmake
rm ./AppDir/usr/bin/dolphin-mpn-nogui
rm ./AppDir/usr/bin/dolphin-tool
mv ./AppDir/usr/share/dolphin-mpn/sys ./AppDir/usr/bin/Sys
rm -rf ./AppDir/usr/share/dolphin-mpn

# Ensure desktop file exists and fix it
if [ -f ./AppDir/usr/share/applications/dolphin-mpn.desktop ]; then
    sed -i 's/env QT_QPA_PLATFORM=xcb dolphin-mpn/dolphin-mpn/g' ./AppDir/usr/share/applications/dolphin-mpn.desktop
else
    echo "ERROR: Desktop file not found at ./AppDir/usr/share/applications/dolphin-mpn.desktop"
    exit 1
fi

# Prepare Tools for building the AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage
wget -N https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage

chmod a+x linuxdeploy-${ARCH}.AppImage
chmod a+x linuxdeploy-plugin-qt-${ARCH}.AppImage
chmod a+x appimagetool-${ARCH}.AppImage

# Build the AppImage
# Ensure the executable exists
if [ ! -f ./AppDir/usr/bin/dolphin-mpn ]; then
    echo "ERROR: Executable not found at ./AppDir/usr/bin/dolphin-mpn"
    exit 1
fi

./linuxdeploy-${ARCH}.AppImage \
  --appdir AppDir \
  --executable ./AppDir/usr/bin/dolphin-mpn \
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
    cp ./AppDir/usr/share/applications/dolphin-mpn.desktop ./AppDir/
fi

./appimagetool-${ARCH}.AppImage ./AppDir