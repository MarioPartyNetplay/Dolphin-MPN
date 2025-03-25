#!/bin/bash
set -e


# Define package metadata
pkgname="dolphin-mpn"
commit_hash=$(git rev-parse --short=7 HEAD)
pkgver="${commit_hash}"
pkgrel=1
arch="x86_64"
pkgdesc="A GameCube and Wii emulator optimized for Mario Party Netplay"
depends=("glibc" "sdl2" "ffmpeg" "qt6-base" "libx11" "libxi" "libxrandr" "zlib" "libevdev")
license=("GPL2")

# Convert generated archive to Arch format
mkdir -p "${pkgname}-${pkgver}-${arch}"
tar -xzf DolphinMPN-*.tar.gz -C "${pkgname}-${pkgver}-${arch}"
mv "${pkgname}-${pkgver}-${arch}/DolphinMPN-archLinux" "${pkgname}-${pkgver}-${arch}"
# Create .PKGINFO file
cat <<EOF > "${pkgname}-${pkgver}-${arch}/.PKGINFO"
pkgname=${pkgname}
pkgver=${pkgver}-${pkgrel}
arch=${arch}
pkgdesc=${pkgdesc}
license=('GPL2')
depends=(${depends[@]})
EOF

# Compress into a valid Arch package format
tar -cJf "DolphinMPN-archLinux.pkg.tar.zst" -C "${pkgname}-${pkgver}-${arch}" .