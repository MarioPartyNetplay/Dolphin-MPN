#!/bin/bash

# macOS Build Preparation Script
# This script builds dependencies needed for macOS builds (SDL, Qt, MoltenVK)

set -e

# Configuration - can be overridden by environment variables
ARCH="${CMAKE_OSX_ARCHITECTURES:-x86_64}"
INSTALLDIR="${MACOS_DEPS_DIR:-$HOME/deps}"
MOLTENVK_DIR="${MOLTENVK_DIR:-$HOME/moltenvk}"
BUILD_MOLTENVK="${BUILD_MOLTENVK:-true}"
MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.00}"

# Qt version
QT="${QT_VERSION:-6.2.10}"
QT_SUFFIX="${QT_SUFFIX:--opensource}"

# SDL version
SDL="${SDL_VERSION:-SDL2-2.30.9}"

echo "=== macOS Build Preparation ==="
echo "Architecture: $ARCH"
echo "Install directory: $INSTALLDIR"
echo "Build MoltenVK: $BUILD_MOLTENVK"

# Set architecture flags
export ARCHFLAGS="-arch $ARCH"
export CMAKE_OSX_ARCHITECTURES="$ARCH"
export MACOSX_DEPLOYMENT_TARGET

# Get repo root for use in functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

NPROCS="$(getconf _NPROCESSORS_ONLN)"

# Function to build dependencies
build_dependencies() {
    echo "=== Building Dependencies ==="
    
    # Ensure we're in repo root
    cd "$REPO_ROOT"
    
    mkdir -p deps-build
    cd deps-build

    export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"
    export LDFLAGS="-L$INSTALLDIR/lib -dead_strip -arch $ARCH $LDFLAGS"
    export CFLAGS="-I$INSTALLDIR/include -Os -arch $ARCH $CFLAGS"
    export CXXFLAGS="-I$INSTALLDIR/include -Os -arch $ARCH $CXXFLAGS"

    echo "Downloading dependencies..."
    curl -L \
      -O "https://libsdl.org/release/$SDL.tar.gz" \
      -O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qtbase-everywhere$QT_SUFFIX-src-$QT.tar.xz" \
      -O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere$QT_SUFFIX-src-$QT.tar.xz" \
      -O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qttools-everywhere$QT_SUFFIX-src-$QT.tar.xz" \
      -O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere$QT_SUFFIX-src-$QT.tar.xz"
    
    echo "Installing SDL..."
    tar xf "$SDL.tar.gz"
    cd "$SDL"
    CC="clang -arch $ARCH" ./configure --prefix "$INSTALLDIR" --without-x
    make "-j$NPROCS"
    make install
    cd ..

    echo "Installing Qt Base..."
    tar xf "qtbase-everywhere$QT_SUFFIX-src-$QT.tar.xz"
    cd "qtbase-everywhere-src-$QT"
    cmake -B build -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
          -DCMAKE_PREFIX_PATH="$INSTALLDIR" \
          -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
          -DCMAKE_BUILD_TYPE=Release \
          -DFEATURE_optimize_size=ON \
          -DFEATURE_dbus=OFF \
          -DFEATURE_framework=OFF \
          -DFEATURE_icu=OFF \
          -DFEATURE_opengl=OFF \
          -DFEATURE_printsupport=OFF \
          -DFEATURE_sql=OFF \
          -DFEATURE_gssapi=OFF \
          -DFEATURE_system_png=OFF \
          -DFEATURE_system_jpeg=OFF \
          -DCMAKE_MESSAGE_LOG_LEVEL=STATUS
    make -C build "-j$NPROCS"
    make -C build install
    cd ..

    echo "Installing Qt SVG..."
    tar xf "qtsvg-everywhere$QT_SUFFIX-src-$QT.tar.xz"
    cd "qtsvg-everywhere-src-$QT"
    cmake -B build -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
          -DCMAKE_PREFIX_PATH="$INSTALLDIR" \
          -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
          -DCMAKE_BUILD_TYPE=Release
    make -C build "-j$NPROCS"
    make -C build install
    cd ..

    echo "Installing Qt Tools..."
    tar xf "qttools-everywhere$QT_SUFFIX-src-$QT.tar.xz"
    cd "qttools-everywhere-src-$QT"

    # Apply patch for linguist
    patch -u src/linguist/CMakeLists.txt <<EOF
--- src/linguist/CMakeLists.txt
+++ src/linguist/CMakeLists.txt
@@ -14,7 +14,7 @@
 add_subdirectory(lrelease-pro)
 add_subdirectory(lupdate)
 add_subdirectory(lupdate-pro)
-if(QT_FEATURE_process AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND NOT no-png)
+if(QT_FEATURE_process AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND TARGET Qt::PrintSupport AND NOT no-png)
     add_subdirectory(linguist)
 endif()
EOF

    cmake -B build -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
          -DCMAKE_PREFIX_PATH="$INSTALLDIR" \
          -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
          -DCMAKE_BUILD_TYPE=Release \
          -DFEATURE_assistant=OFF \
          -DFEATURE_clang=OFF \
          -DFEATURE_designer=OFF \
          -DFEATURE_kmap2qmap=OFF \
          -DFEATURE_pixeltool=OFF \
          -DFEATURE_pkg_config=OFF \
          -DFEATURE_qev=OFF \
          -DFEATURE_qtattributionsscanner=OFF \
          -DFEATURE_qtdiag=OFF \
          -DFEATURE_qtplugininfo=OFF \
          -DFEATURE_linguist=OFF
    make -C build "-j$NPROCS"
    make -C build install
    cd ..

    echo "Installing Qt Translations..."
    tar xf "qttranslations-everywhere$QT_SUFFIX-src-$QT.tar.xz"
    cd "qttranslations-everywhere-src-$QT"
    cmake -B build -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
          -DCMAKE_PREFIX_PATH="$INSTALLDIR" \
          -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
          -DCMAKE_BUILD_TYPE=Release
    make -C build "-j$NPROCS"
    cd ..

    echo "Cleaning up..."
    cd "$REPO_ROOT"
    rm -rf deps-build
    
    echo "Dependencies built successfully in $INSTALLDIR"
}

# Function to build MoltenVK
build_moltenvk() {
    echo "=== Building MoltenVK ==="
    
    # Ensure we're in repo root
    cd "$REPO_ROOT"
    
    # Get MoltenVK version from CMakeLists.txt
    if [ ! -f "Externals/MoltenVK/CMakeLists.txt" ]; then
        echo "ERROR: Externals/MoltenVK/CMakeLists.txt not found"
        echo "Skipping MoltenVK build"
        return
    fi
    
    MVK_VER="$(sed -nr 's/^.*set\(MOLTENVK_VERSION "([^"]+)".*$/\1/p' Externals/MoltenVK/CMakeLists.txt)"
    
    if [ -z "$MVK_VER" ]; then
        echo "ERROR: Failed to parse MoltenVK version from CMakeLists"
        echo "Skipping MoltenVK build"
        return
    fi
    
    echo "Building MoltenVK version: $MVK_VER"
    
    git clone --depth 1 --branch "$MVK_VER" https://github.com/KhronosGroup/MoltenVK.git mvk-build
    
    pushd mvk-build
    
    if [ -d "../Externals/MoltenVK/patches" ]; then
        git apply ../Externals/MoltenVK/patches/*.patch
    fi
    
    ./fetchDependencies --macos
    
    make macos
    
    ls -l Package/Release/MoltenVK/dynamic/*
    
    chmod 755 Package/Release/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib
    
    mkdir -p "$MOLTENVK_DIR/lib/"
    
    mv Package/Release/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib "$MOLTENVK_DIR/lib/"
    
    popd
    
    cd "$REPO_ROOT"
    rm -rf mvk-build
    
    echo "MoltenVK built successfully in $MOLTENVK_DIR"
}

# Main execution
main() {
    # Ensure we're in the repository root (REPO_ROOT is set at script level)
    cd "$REPO_ROOT"
    
    # Check if dependencies already exist (unless SKIP_DEPS is explicitly set)
    if [ "${SKIP_DEPS:-false}" != "true" ]; then
        if [ -d "$INSTALLDIR/lib" ] && [ -f "$INSTALLDIR/lib/libQt6Core.dylib" ]; then
            echo "Dependencies already exist in $INSTALLDIR"
            echo "Skipping dependency build. Set FORCE_REBUILD=1 to rebuild."
            if [ "${FORCE_REBUILD:-0}" != "1" ]; then
                SKIP_DEPS=true
            fi
        fi
        
        if [ "${SKIP_DEPS:-false}" != "true" ]; then
            build_dependencies
        fi
    fi
    
    if [ "$BUILD_MOLTENVK" = "true" ]; then
        # Check if MoltenVK already exists (unless SKIP_MOLTENVK is explicitly set)
        if [ "${SKIP_MOLTENVK:-false}" != "true" ]; then
            if [ -f "$MOLTENVK_DIR/lib/libMoltenVK.dylib" ]; then
                echo "MoltenVK already exists in $MOLTENVK_DIR"
                echo "Skipping MoltenVK build. Set FORCE_REBUILD=1 to rebuild."
                if [ "${FORCE_REBUILD:-0}" != "1" ]; then
                    SKIP_MOLTENVK=true
                fi
            fi
            
            if [ "${SKIP_MOLTENVK:-false}" != "true" ]; then
                build_moltenvk
            fi
        fi
    fi
    
    echo ""
    echo "=== Build Preparation Complete ==="
    echo "Dependencies: $INSTALLDIR"
    if [ "$BUILD_MOLTENVK" = "true" ]; then
        echo "MoltenVK: $MOLTENVK_DIR"
    fi
    echo ""
    echo "You can now configure CMake with:"
    if [ "$BUILD_MOLTENVK" = "true" ]; then
        echo "  -DCMAKE_PREFIX_PATH=\"$INSTALLDIR;$MOLTENVK_DIR\""
    else
        echo "  -DCMAKE_PREFIX_PATH=\"$INSTALLDIR\""
    fi
    echo "  -DCMAKE_OSX_ARCHITECTURES=$ARCH"
}

main "$@"

