#!/bin/bash
set -e

# Simple status function for CI
print_status() {
    echo "[INFO] $1"
}

# Check for required tools
check_dependencies() {
    local missing_deps=()
    
    for dep in "cmake" "git" "ninja" "tar" "xz"; do
        if ! command -v "$dep" &> /dev/null; then
            missing_deps+=("$dep")
        fi
    done
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        echo "[ERROR] Missing required dependencies: ${missing_deps[*]}"
        exit 1
    fi
}

# Get version information
get_version_info() {
    if [[ -d .git ]]; then
        commit_hash=$(git rev-parse --short=7 HEAD)
        pkgver="${commit_hash}"
        print_status "Building from git commit: ${commit_hash}"
    else
        pkgver="5.0.0"
        print_status "Using default version: ${pkgver}"
    fi
}

# Build the project
build_project() {
    print_status "Building dolphin-mpn..."
    
    # Create build directory
    mkdir -p build
    cd build
    
    # Configure with CMake
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DENABLE_QT=ON \
        -DENABLE_NOGUI=ON \
        -DUSE_SYSTEM_LIBS=OFF \
        -DUSE_SYSTEM_ICONV=ON \
        -DUSE_SYSTEM_BZIP2=ON \
        -DUSE_SYSTEM_CURL=ON \
        -G Ninja
    
    # Build
    ninja -j$(nproc)
    
    cd ..
}

# Create Arch package
create_arch_package() {
    print_status "Creating Arch Linux package..."
    
    # Define package metadata
    pkgname="dolphin-mpn"
    pkgrel=1
    arch="x86_64"
    pkgdesc="A GameCube and Wii emulator optimized for Mario Party Netplay"
    depends=("glibc" "sdl2" "ffmpeg" "qt6-base" "libx11" "libxi" "libxrandr" "zlib" "libevdev" "alsa-lib" "bluez-libs" "cubeb" "enchant" "fmt" "hidapi" "libpulse" "lzo" "mbedtls" "miniupnpc" "pugixml" "sfml" "zstd")
    license=("GPL2")
    
    # Create package directory
    local package_dir="${pkgname}-${pkgver}-${arch}"
    mkdir -p "${package_dir}"
    
    # Install to package directory
    cd build
    DESTDIR="../${package_dir}" ninja install
    cd ..
    
    # Create .PKGINFO file
    cat <<EOF > "${package_dir}/.PKGINFO"
pkgname=${pkgname}
pkgver=${pkgver}-${pkgrel}
arch=${arch}
pkgdesc=${pkgdesc}
license=('GPL2')
depends=(${depends[@]})
EOF

    # Create package
    local package_name="DolphinMPN-archLinux-${pkgver}.pkg.tar.zst"
    tar -cJf "${package_name}" -C "${package_dir}" .
    
    print_status "Package created: ${package_name}"
    
    # Cleanup
    rm -rf "${package_dir}"
}

# Main execution
main() {
    print_status "Starting Arch Linux package build for dolphin-mpn..."
    
    check_dependencies
    get_version_info
    build_project
    create_arch_package
    
    print_status "Build completed successfully!"
}

# Run main function
main "$@"