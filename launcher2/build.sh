#!/usr/bin/env bash
# Builds the ACG1PLUS launcher for both systems on a Linux machine (developer tool; the
# sandbox uses it to make the files uploaded to a Release).
#
#   launcher2/build.sh WORK_DIR [linux|windows|both]
#
# WORK_DIR holds the downloads (kept between runs) and the results:
#   WORK_DIR/out/linux/ACG1PLUS        Steam Deck / Linux
#   WORK_DIR/out/windows/ACG1PLUS.exe  Windows
#
# What it downloads, each checked against its pinned SHA-256:
#   - cmake-clang-v1 v1.0.14 linux-x64: CMake, Ninja, Clang, the SteamOS build sysroot and a
#     static SDL3 3.4.10, the same pack scripts/linux/build.sh builds the game with.
#   - cmake-clang-v1 v1.0.14 windows-x64: only its deps/ folder (the static SDL3 3.4.10 for
#     Windows that the Windows game uses).
#   - llvm-mingw 20261117 (ucrt, Linux host): the compiler that makes Windows programs on Linux.
#
# The source zip that goes with the launcher (tools/release/make_source_zip.py) is
# baked in from these, when set (launcher2/release.sh sets them):
#   ACG_SOURCE_URL ACG_SOURCE_SHA256 ACG_SOURCE_SIZE ACG_SOURCE_VERSION
# Without them it is a developer build that needs --source-zip FILE.
# ACG_REPO (owner/name, default Hatdagger/ACG1PLUS): where UPDATES looks (0.2.4).
set -euo pipefail

WORK="${1:?usage: launcher2/build.sh WORK_DIR [linux|windows|both]}"
WHICH="${2:-both}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"
DL="$WORK/downloads"
mkdir -p "$DL"

PACK_VERSION="v1.0.14"
PACK_BASE="https://github.com/RetroPortingToolKit/RetroPorting-Toolchains/releases/download/$PACK_VERSION"
LINUX_PACK_SHA256="597c8d343a3cf02ba6f6b2ae7cf6fe2fef125dde8feff62a144e8dd3da3d484e"
WINDOWS_PACK_SHA256="28da9742385e7ff875b3d9311e8ed89dbdc84f27b6ecba2bc0d0acc11f6d2b4d"
MINGW_VERSION="20261117"
MINGW_NAME="llvm-mingw-$MINGW_VERSION-ucrt-ubuntu-22.04-x86_64"
MINGW_URL="https://github.com/mstorsjo/llvm-mingw/releases/download/$MINGW_VERSION/$MINGW_NAME.tar.xz"
MINGW_SHA256="${ACG_MINGW_SHA256:-e7a15a49a2ea95e9f3d8a9d0cfabe5052a4b10fb4777cc54de5a8c27b64bb701}"

fail() { echo "build.sh: $*" >&2; exit 1; }

SOURCE_ARGS=(-DACG_SOURCE_URL="${ACG_SOURCE_URL:-}" -DACG_SOURCE_SHA256="${ACG_SOURCE_SHA256:-}"
             -DACG_SOURCE_SIZE="${ACG_SOURCE_SIZE:-0}" -DACG_SOURCE_VERSION="${ACG_SOURCE_VERSION:-}"
             -DACG_REPO="${ACG_REPO:-Hatdagger/ACG1PLUS}")

fetch() {   # url file sha256
    local url="$1" file="$2" sha="$3"
    if [ -f "$file" ] && [ "$(sha256sum "$file" | cut -d' ' -f1)" = "$sha" ]; then return; fi
    echo "Downloading $url" >&2   # stderr: linux_pack() returns its path on stdout
    curl -L --fail --retry 3 -o "$file.part" "$url" || fail "download failed: $url"
    local got
    got="$(sha256sum "$file.part" | cut -d' ' -f1)"
    [ "$got" = "$sha" ] || { rm -f "$file.part"; fail "checksum mismatch for $url (got $got)"; }
    mv "$file.part" "$file"
}

[ -f "$HERE/../recomp-ui/src/third_party/imgui/imgui.cpp" ] ||
    fail "recomp-ui is missing: git submodule update --init recomp-ui"

linux_pack() {
    local pack="$DL/linux-pack"
    if [ ! -f "$pack/.version" ] || [ "$(cat "$pack/.version")" != "$PACK_VERSION" ]; then
        fetch "$PACK_BASE/cmake-clang-v1-linux-x64.zip" "$DL/cmake-clang-v1-linux-x64-$PACK_VERSION.zip" "$LINUX_PACK_SHA256"
        rm -rf "$pack"; mkdir -p "$pack"
        unzip -q "$DL/cmake-clang-v1-linux-x64-$PACK_VERSION.zip" -d "$pack" >&2
        echo "$PACK_VERSION" > "$pack/.version"
    fi
    echo "$pack"
}

build_linux() {
    local pack; pack="$(linux_pack)"
    (
        # The pack's own setup: its Clang, which reads clang.cfg and builds against the SteamOS
        # sysroot, so the result runs on a stock Deck (glibc 2.35 at most).
        . "$pack/env.sh"
        cmake -S "$HERE" -B "$WORK/build-linux" -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_C_COMPILER="$pack/bin/clang" -DCMAKE_CXX_COMPILER="$pack/bin/clang++" \
            -DCMAKE_SYSROOT="$pack/sysroot" -DSDL3_DIR="$pack/deps/lib/cmake/SDL3" "${SOURCE_ARGS[@]}"
        cmake --build "$WORK/build-linux"
    )
    mkdir -p "$WORK/out/linux"
    cp "$WORK/build-linux/ACG1PLUS" "$WORK/out/linux/ACG1PLUS"
    chmod +x "$WORK/out/linux/ACG1PLUS"
    echo "Linux:   $WORK/out/linux/ACG1PLUS"
}

build_windows() {
    local pack; pack="$(linux_pack)"   # for CMake and Ninja
    local winsdl="$DL/windows-deps"
    if [ ! -f "$winsdl/.version" ] || [ "$(cat "$winsdl/.version")" != "$PACK_VERSION" ]; then
        fetch "$PACK_BASE/cmake-clang-v1-windows-x64.zip" "$DL/cmake-clang-v1-windows-x64-$PACK_VERSION.zip" "$WINDOWS_PACK_SHA256"
        rm -rf "$winsdl"; mkdir -p "$winsdl"
        unzip -q "$DL/cmake-clang-v1-windows-x64-$PACK_VERSION.zip" 'deps/*' -d "$winsdl"
        echo "$PACK_VERSION" > "$winsdl/.version"
    fi
    local mingw="$DL/$MINGW_NAME"
    if [ ! -x "$mingw/bin/x86_64-w64-mingw32-clang" ]; then
        fetch "$MINGW_URL" "$DL/$MINGW_NAME.tar.xz" "$MINGW_SHA256"
        tar -xf "$DL/$MINGW_NAME.tar.xz" -C "$DL"
    fi
    "$pack/bin/cmake" -S "$HERE" -B "$WORK/build-windows" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_MAKE_PROGRAM="$pack/bin/ninja" \
        -DCMAKE_TOOLCHAIN_FILE="$HERE/cmake/mingw-w64-x86_64.cmake" -DLLVM_MINGW="$mingw" \
        -DSDL3_DIR="$winsdl/deps/lib/cmake/SDL3" "${SOURCE_ARGS[@]}"
    "$pack/bin/cmake" --build "$WORK/build-windows"
    mkdir -p "$WORK/out/windows"
    cp "$WORK/build-windows/ACG1PLUS.exe" "$WORK/out/windows/ACG1PLUS.exe"
    echo "Windows: $WORK/out/windows/ACG1PLUS.exe"
}

case "$WHICH" in
    linux) build_linux ;;
    windows) build_windows ;;
    both) build_linux; build_windows ;;
    *) fail "unknown target: $WHICH" ;;
esac
