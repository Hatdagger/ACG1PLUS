#!/usr/bin/env bash
# ACG1PLUS: build the game on a Steam Deck (or another Linux PC) from your own disc.
#
# Normally started by "Build ACG1PLUS.sh" in the game folder, which first fetches
# this repository into RecompData/build/source/. The Linux twin of
# scripts/windows/build.ps1; see docs/building.md.
#
# Everything happens inside the game folder (portable install, owner decision
# 2026-11-26). Nothing is installed, the Deck's read-only system is not
# touched, and nothing is written to ~/.local, ~/.config or ~/.cache:
#
#   <game folder>/
#     Start ACG1PLUS.sh       start the Deck game (written here at the end)
#     Build ACG1PLUS.sh       build / update
#     Disc/                   your Project Phantasma USA .cue + .bin
#     Memory Card/  Mods/     yours, never touched by a build
#     RecompData/
#       linux/                the game this script makes
#       windows/              the Windows game, left alone
#       launcher/             the launcher's own small files, left alone
#       build/
#         source/             this repository + the framework
#         tools-linux/        the compiler pack (CMake, Clang, Python, SteamOS build files)
#         temp/  logs/        temporary files and build logs
#
# No game code is downloaded: the game is translated from your disc here.
#
#   bash build.sh GAME_FOLDER [--force] [--disc FILE.cue]
#     --force   translate the game again even if nothing changed (slow)
#     --disc    use this .cue instead of looking in Disc/
set -u
set -o pipefail

GAME_ROOT="${1:?usage: build.sh GAME_FOLDER [--force] [--disc FILE.cue]}"
shift
FORCE=0
DISC_ARG=""
while [ $# -gt 0 ]; do
    case "$1" in
        --force) FORCE=1 ;;
        --disc) DISC_ARG="${2:-}"; shift ;;
        *) echo "unknown option: $1"; exit 2 ;;
    esac
    shift
done

# ---- pinned download (version + SHA-256, checked before use) -----------------
# The compiler pack the framework pin expects (psxrecomp 44a401b7 asks for
# 1.0.14 or newer). One fixed version, so every build of this game is made by
# the same compiler (netplay needs identical builds). 1.0.14 is also the first
# pack with the SteamOS build files (system headers the Deck doesn't have).
PACK_VERSION="v1.0.14"
PACK_URL="https://github.com/RetroPortingToolKit/RetroPorting-Toolchains/releases/download/$PACK_VERSION/cmake-clang-v1-linux-x64.zip"
PACK_SHA256="597c8d343a3cf02ba6f6b2ae7cf6fe2fef125dde8feff62a144e8dd3da3d484e"
EXE_NAME="ACG1PLUS_Game"

# ---- folders ----------------------------------------------------------------
GAME_ROOT="$(cd "$GAME_ROOT" && pwd)" || exit 1
DATA="$GAME_ROOT/RecompData"
BUILD="$DATA/build"
SRC="$BUILD/source"
TOOLS="$BUILD/tools-linux"
PACK="$TOOLS/toolchain"
TEMP="$BUILD/temp"
LOGS="$BUILD/logs"
GAME="$DATA/linux"
mkdir -p "$DATA" "$BUILD" "$TOOLS" "$TEMP" "$LOGS" "$GAME_ROOT/Disc" "$GAME_ROOT/Memory Card" \
         "$GAME_ROOT/Mods" || { echo "Could not make folders in $GAME_ROOT"; exit 1; }

STAMP="$(date +%Y-%m-%d_%H-%M-%S)"
LOG="$LOGS/build-linux-$STAMP.txt"
exec > >(tee -a "$LOG") 2>&1
START_SECONDS=$SECONDS

step() { echo; echo "==> $*"; }
fail() {
    echo
    echo "BUILD STOPPED: $*"
    echo "The full log is in $LOG"
    exit 1
}

# ---- keep every tool inside the folder ----------------------------------------
# Temporary files, compiler caches, the framework's toolchain lookups, and the
# places Linux programs keep caches and settings all point here, for this run
# only.
export TMPDIR="$TEMP" TEMP="$TEMP" TMP="$TEMP"
export CCACHE_DIR="$TOOLS/ccache"
export RETCOMM_TOOLCHAIN_DIR="$PACK"
export PSXRECOMP_TOOLCHAIN_DIR="$PACK"
export RETCOMM_TOOLCHAIN_CACHE="$TOOLS/cache"
export RETCOMM_DATA_HOME="$TOOLS/retcomm"
export RETCOMM_TOOLCHAIN_SKIP_UPDATE=1
export PSXRECOMP_TOOLCHAIN_READONLY=1
export XDG_CACHE_HOME="$TEMP/xdg/cache"
export XDG_CONFIG_HOME="$TEMP/xdg/config"
export XDG_DATA_HOME="$TEMP/xdg/data"
export XDG_STATE_HOME="$TEMP/xdg/state"
mkdir -p "$XDG_CACHE_HOME" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_STATE_HOME"
export PYTHONNOUSERSITE=1 PIP_NO_INDEX=1
export GIT_CONFIG_NOSYSTEM=1
# A folder on a USB drive (exFAT) has no owner and no "program" flag; tell git
# both are fine, for this run only (the same lines as in "Build ACG1PLUS.sh").
export GIT_CONFIG_COUNT=2
export GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0='*'
export GIT_CONFIG_KEY_1=core.fileMode GIT_CONFIG_VALUE_1=false
export PSXRECOMP_PROJECT_ROOT="$SRC"

# Places outside the folder this build must not touch. Each one that is missing
# now is checked again at the end.
WATCH=("$HOME/.local/share/retcomm" "$HOME/.local/share/psxrecomp" "$HOME/.cache/retcomm"
       "$HOME/.cache/psxrecomp" "$HOME/.cache/ccache" "$HOME/.config/ccache" "$HOME/.config/ACG1PLUS"
       "$HOME/.config/recomp-ui" "$HOME/.local/share/ACG1PLUS" "$HOME/.local/share/Armored-Core-Gen-1")
WATCH_NEW=()
for w in "${WATCH[@]}"; do [ -e "$w" ] || WATCH_NEW+=("$w"); done

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
    else python3 -I -c 'import hashlib,sys;print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$1"; fi
}

# ---- 0. can programs run from this folder? ---------------------------------------
# Some drives are mounted so that nothing on them may run as a program ("noexec").
# The compiler and the game both have to, so find out now instead of after a
# long download. (ROUND1 test #32.)
step '0/6  Checking that programs can run from this folder'
PROBE="$TEMP/acg-probe.sh"
printf '#!/bin/sh\necho ok\n' > "$PROBE" || fail "could not write into $TEMP"
chmod +x "$PROBE" 2>/dev/null || true
if [ "$("$PROBE" 2>/dev/null)" != "ok" ]; then
    rm -f "$PROBE"
    fail "programs can't run from this drive ($(df -P "$GAME_ROOT" | awk 'NR==2{print $6}')).
The drive is probably mounted with 'noexec', or its file system can't mark files as programs.
Put the game folder on a drive formatted exFAT and mounted by the Deck itself (Desktop Mode
mounts USB drives at /run/media/deck/...), or on the Deck's own storage, and run this again."
fi
rm -f "$PROBE"
echo "Yes ($(df -PT "$GAME_ROOT" 2>/dev/null | awk 'NR==2{print $2}') at $(df -P "$GAME_ROOT" | awk 'NR==2{print $6}'))."

# ---- 1. compiler pack (cmake, ninja, clang, Python, SteamOS build files) ----------
step '1/6  Compiler pack'
STAMP_FILE="$PACK/.acg-pack-version"
if [ -x "$PACK/bin/cmake" ] && [ -x "$PACK/python/bin/python3" ] && [ -f "$STAMP_FILE" ] &&
   [ "$(cat "$STAMP_FILE")" = "$PACK_VERSION" ]; then
    echo "Compiler pack $PACK_VERSION is already here."
else
    ZIP="$TOOLS/cmake-clang-v1-linux-x64-$PACK_VERSION.zip"
    if [ ! -f "$ZIP" ] || [ "$(sha256_of "$ZIP")" != "$PACK_SHA256" ]; then
        echo "Downloading $PACK_URL (about 830 MB)"
        curl -L --fail --retry 3 -o "$ZIP" "$PACK_URL" || fail "download failed: $PACK_URL"
        GOT="$(sha256_of "$ZIP")"
        if [ "$GOT" != "$PACK_SHA256" ]; then
            rm -f "$ZIP"
            fail "download does not match its checksum: $PACK_URL (got $GOT)"
        fi
    fi
    rm -rf "$PACK"
    mkdir -p "$PACK"
    echo 'Unpacking (about 2.6 GB)...'
    if command -v unzip >/dev/null 2>&1; then
        unzip -q -o "$ZIP" -d "$PACK" || fail 'could not unpack the compiler pack'
    elif command -v bsdtar >/dev/null 2>&1; then
        bsdtar -xf "$ZIP" -C "$PACK" || fail 'could not unpack the compiler pack'
    else
        python3 -I -c 'import sys,zipfile;zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' "$ZIP" "$PACK" ||
            fail 'could not unpack the compiler pack'
    fi
    # Mark the pack's programs as programs again (some unpackers drop that). On
    # exFAT this does nothing and isn't needed: every file there may run.
    python3 -I - "$ZIP" "$PACK" <<'PY' 2>/dev/null || true
import os, stat, sys, zipfile
for i in zipfile.ZipFile(sys.argv[1]).infolist():
    mode = i.external_attr >> 16
    if mode & 0o111 and not stat.S_ISDIR(mode):
        try:
            os.chmod(os.path.join(sys.argv[2], i.filename), 0o755)
        except OSError:
            pass
PY
    [ -x "$PACK/bin/cmake" ] || fail 'the compiler pack is incomplete after unpacking'
    echo "$PACK_VERSION" > "$STAMP_FILE"
    rm -f "$ZIP"
fi
# The pack's own session setup: PATH, CC/CXX, its Python, SDL3, and the build
# sysroot that stands in for the system headers SteamOS doesn't have.
# shellcheck disable=SC1091
. "$PACK/env.sh"
PYTHON="$PACK/python/bin/python3"
"$PACK/bin/clang" --version | head -1 || fail 'the compiler in the pack does not run on this system'
# The framework compiles the disc's code with the first of gcc, cc, clang it
# finds. A stock Deck has no gcc (so it takes the pack's), but a Linux PC may
# have one, and then two machines would build different games (netplay needs
# identical builds). A tiny stand-in named gcc, first on PATH for this run only,
# sends those calls to the pack's Clang everywhere.
SHIM="$TOOLS/shim"
mkdir -p "$SHIM"
printf '#!/bin/sh\nexec "%s/bin/clang" "$@"\n' "$PACK" > "$SHIM/gcc"
chmod +x "$SHIM/gcc" 2>/dev/null || true
export PATH="$SHIM:$PATH"
[ "$(command -v gcc)" = "$SHIM/gcc" ] || fail 'could not put the compiler stand-in first on PATH'
command -v git >/dev/null 2>&1 || fail 'git was not found. Start the build with "Build ACG1PLUS.sh".'

CLI="$SRC/psxrecomp/psxrecomp_cli.py"
TOML="$SRC/game.toml"
[ -f "$CLI" ] || fail "the source is incomplete: $CLI is missing"

# ---- 2. your disc -------------------------------------------------------------------
step '2/6  Your disc (Project Phantasma USA, SLUS-00670)'
FOUND=""
check_cue() {
    echo "Checking $1"
    "$PYTHON" "$CLI" verify-disc --config "$TOML" --disc "$1"
}
if [ -n "$DISC_ARG" ]; then
    check_cue "$DISC_ARG" && FOUND="$DISC_ARG"
else
    for place in "$GAME_ROOT/Disc" "$GAME_ROOT"; do
        while IFS= read -r -d '' cue; do
            if check_cue "$cue"; then FOUND="$cue"; break; fi
        done < <(find "$place" -maxdepth 1 -type f -iname '*.cue' -print0 2>/dev/null | sort -z)
        [ -n "$FOUND" ] && break
    done
fi
[ -n "$FOUND" ] || fail "no matching disc found. Put your Project Phantasma USA disc image (.cue + .bin,
one data track, Redump size 454,072,416 bytes) in $GAME_ROOT/Disc"
echo "Using $FOUND"

# ---- 3. translate the game from the disc --------------------------------------------
# The main program, then (aot/overlays.json) the code the game loads from the
# disc while playing. Both land in source/generated/, which never leaves this PC.
step '3/6  Translating the game from your disc (10-30 minutes the first time)'
GEN=("$CLI" generate --config "$TOML" --project-root "$SRC" --disc "$FOUND")
[ "$FORCE" = 1 ] && GEN+=(--force-aot-static)
"$PYTHON" "${GEN[@]}" || fail "psxrecomp generate (exit code $?)"
# Generate copies the disc into source/disc/ the first time. Only the boot
# program it extracted is needed again (next builds read your Disc/ copy).
if [ -d "$SRC/disc" ]; then
    find "$SRC/disc" -maxdepth 1 -type f \( -iname '*.bin' -o -iname '*.cue' \) -delete
fi

# ---- 4. compile -------------------------------------------------------------------------
# Two optional libraries are switched off so that every Linux machine builds the
# same game as a stock Deck, using only what is in the compiler pack:
# - OpenSSL (file checksums): if a PC has its developer files, the framework
#   finds them outside the pack, and the build breaks; without it the framework
#   uses its own checksum code.
# - FreeType (colour emoji in menus, plus HarfBuzz with it): same story; without
#   it emoji are drawn as outlines, as on a stock Deck.
step '4/6  Compiling (20-60 minutes the first time, faster after)'
"$PYTHON" "$CLI" rebuild --config "$TOML" --project-root "$SRC" \
    --build-dir build-release --target psx-runtime --exe-basename "$EXE_NAME" \
    --no-toolchain-download --no-pgo \
    --cmake-extra=-DPSX_SDL_BACKEND=SDL3 --cmake-extra=-DPSX_PGXP_VARIANT=OFF \
    --cmake-extra=-DPSX_RELEASE_CHANNEL=playtest \
    --cmake-extra=-DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON \
    --cmake-extra=-DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON || fail "psxrecomp rebuild (exit code $?)"
OUT="$SRC/build-release"
[ -f "$OUT/$EXE_NAME" ] || fail "the build finished but $OUT/$EXE_NAME is missing"

# ---- 5. put the game into RecompData/linux -------------------------------------------
step '5/6  Putting the game in place'
mkdir -p "$GAME"
cp -f "$OUT/$EXE_NAME" "$GAME/" || fail "could not copy $EXE_NAME"
for f in game.toml game_options.toml psx_game_version.txt; do
    [ -f "$OUT/$f" ] && cp -f "$OUT/$f" "$GAME/"
done
# Folders the runtime reads beside the program: each becomes an exact copy of
# the build's (stale files removed). The player's own files (settings.toml,
# saves, caches, keyboard profiles) are outside them; mods/installed holds the
# player's installed mods and is never replaced.
mirror() {   # mirror FROM TO [KEEP_NAME]
    local from="$1" to="$2" keep="${3:-}"
    [ -d "$from" ] || return 0
    mkdir -p "$to"
    find "$to" -mindepth 1 -maxdepth 1 ! -name "${keep:-.acg-never}" -exec rm -rf {} +
    (cd "$from" && find . -mindepth 1 -maxdepth 1 ! -name "${keep:-.acg-never}" -exec cp -R {} "$to/" \;) ||
        fail "could not copy $from"
}
mirror "$OUT/assets" "$GAME/assets"
mirror "$OUT/bios" "$GAME/bios"
mirror "$OUT/mods" "$GAME/mods" installed
mirror "$OUT/overlay_toolchain" "$GAME/overlay_toolchain"
# Third-party notices (compiler runtime, SDL, OpenBIOS ...), from the framework.
mirror "$SRC/psxrecomp/runtime/licenses" "$GAME/licenses"
cp -f "$SRC/Build ACG1PLUS.sh" "$GAME_ROOT/" 2>/dev/null || true
cp -f "$SRC/scripts/linux/start.sh" "$GAME_ROOT/Start ACG1PLUS.sh" || fail 'could not write Start ACG1PLUS.sh'
chmod +x "$GAME_ROOT/Start ACG1PLUS.sh" "$GAME_ROOT/Build ACG1PLUS.sh" "$GAME/$EXE_NAME" 2>/dev/null || true

# Portable: the background overlay compiler is named by a path relative to
# overlay_toolchain/ (the framework accepts that), so the folder can move.
COMPILER_TXT="$GAME/overlay_toolchain/compiler.txt"
if [ -d "$GAME/overlay_toolchain" ] && [ -x "$PACK/bin/clang" ]; then
    REL="../../build/tools-linux/toolchain/bin/clang"
    printf '%s\n' "$REL" > "$COMPILER_TXT"
    echo "Background compiler: $REL (relative, so the folder can be moved)"
fi

# ---- 6. check ----------------------------------------------------------------------------
step '6/6  Checking'
PROBLEMS=()
[ -f "$GAME_ROOT/Start ACG1PLUS.sh" ] || PROBLEMS+=("Start ACG1PLUS.sh is missing")
[ -f "$GAME/$EXE_NAME" ] || PROBLEMS+=("$EXE_NAME is missing")
[ -f "$GAME/bios/openbios.bin" ] || PROBLEMS+=("bios/openbios.bin is missing")
for w in "${WATCH_NEW[@]}"; do
    [ -e "$w" ] && PROBLEMS+=("something wrote $w during the build (it must stay inside the game folder)")
done
[ ${#PROBLEMS[@]} -eq 0 ] || fail "$(printf '%s; ' "${PROBLEMS[@]}")"

MIN=$(( (SECONDS - START_SECONDS) / 60 ))
echo
echo "Done in about $MIN minutes. Start the game with:  bash \"$GAME_ROOT/Start ACG1PLUS.sh\""
echo "Game folder size: $(du -sh "$GAME_ROOT" 2>/dev/null | cut -f1)"
echo "Log: $LOG"
exit 0
