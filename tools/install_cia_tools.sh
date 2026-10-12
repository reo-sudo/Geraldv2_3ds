#!/usr/bin/env bash
# For Debian-based devkitPro container. Builds bannertool and makerom only when missing.
set -euo pipefail
if [[ $(id -u) != 0 ]]; then
    echo 'Run as root inside the devkitPro build container.' >&2; exit 1
fi
if command -v makerom >/dev/null && command -v bannertool >/dev/null; then
    echo 'makerom and bannertool already available'; exit 0
fi
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends git g++ make cmake pkg-config \
    libpng-dev libjpeg-dev libvorbis-dev libogg-dev libsndfile1-dev \
    zlib1g-dev libssl-dev
TMP_TOOLS=$(mktemp -d)
trap 'rm -rf "$TMP_TOOLS"' EXIT
if ! command -v bannertool >/dev/null; then
    git clone --depth 1 https://github.com/carstene1ns/3ds-bannertool.git "$TMP_TOOLS/bannertool"
    cmake -S "$TMP_TOOLS/bannertool" -B "$TMP_TOOLS/bannertool/build" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
    cmake --build "$TMP_TOOLS/bannertool/build" --parallel 2
    cmake --install "$TMP_TOOLS/bannertool/build"
fi
if ! command -v makerom >/dev/null; then
    git clone --depth 1 https://github.com/3DSGuy/Project_CTR.git "$TMP_TOOLS/Project_CTR"
    make -C "$TMP_TOOLS/Project_CTR/makerom" deps -j2
    make -C "$TMP_TOOLS/Project_CTR/makerom" program -j2
    install -m 755 "$TMP_TOOLS/Project_CTR/makerom/bin/makerom" /usr/local/bin/makerom
fi
command -v makerom && command -v bannertool
