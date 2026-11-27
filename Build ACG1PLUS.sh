#!/usr/bin/env bash
# ============================================================================
#  ACG1PLUS - build (or update) the game on a Steam Deck / Linux PC from your
#  own disc. The Linux twin of "Build ACG1PLUS.bat"; both can sit in the same
#  game folder, and the Windows and the Deck game end up side by side.
#
#  First time: put this file in the game folder (an empty folder, or the one
#  the Windows build already made, for example on a USB drive), put your
#  Project Phantasma USA .cue + .bin in a "Disc" folder beside it, then in
#  Desktop Mode open Konsole in that folder and type:
#
#      bash "Build ACG1PLUS.sh"
#
#  (Starting it through bash works even where files can't be marked as
#  programs, e.g. some USB drives.) Later: run it again to update and rebuild.
#
#  Everything stays in this folder (portable): nothing is installed, the
#  Deck's read-only system is not touched, nothing goes to ~/.local, ~/.config
#  or ~/.cache. It downloads only the compiler pack and this project's source.
#  No game code is downloaded: the game is translated from your disc here.
#  See docs/building.md.
# ============================================================================
set -u

# Which version of the project to build.
ACG_BRANCH="main"
ACG_REPO="https://github.com/Hatdagger/ACG1PLUS.git"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/RecompData/build"
SRC="$BUILD/source"
mkdir -p "$BUILD/tools-linux" "$BUILD/temp" "$ROOT/Disc" || {
    echo "Could not make folders in $ROOT (is the drive read-only?)"; exit 1; }
export TMPDIR="$BUILD/temp"
# A folder on a USB drive (exFAT) has no owner; tell git that is fine, for this
# run only (nothing is written to git's settings).
# exFAT also can't store "this file is a program"; don't let git see every file as changed.
export GIT_CONFIG_COUNT=2 GIT_CONFIG_NOSYSTEM=1
export GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0='*'
export GIT_CONFIG_KEY_1=core.fileMode GIT_CONFIG_VALUE_1=false

failed() {
    echo
    echo "The build stopped. The log is in RecompData/build/logs/ (if it got that far)."
    exit 1
}

echo
echo "ACG1PLUS build - game folder: $ROOT"
echo

# ---- git ------------------------------------------------------------------
# SteamOS comes with git. Nothing is installed if it is missing.
if ! command -v git >/dev/null 2>&1; then
    echo "git was not found on this system. SteamOS normally has it; on another Linux, install git"
    echo "with your package manager and run this again."
    failed
fi

# ---- source (this project + the framework, no game code) -------------------
if [ ! -d "$SRC/.git" ]; then
    echo "==> Fetching the source ($ACG_BRANCH)"
    git clone --depth 1 --branch "$ACG_BRANCH" "$ACG_REPO" "$SRC" || failed
else
    echo "==> Updating the source ($ACG_BRANCH)"
    git -C "$SRC" fetch --depth 1 origin "$ACG_BRANCH" || failed
    git -C "$SRC" reset --hard FETCH_HEAD || failed
fi
# --force: the build re-applies our small framework patches itself.
git -C "$SRC" submodule update --init --recursive --depth 1 --force || failed

# ---- the build -------------------------------------------------------------
bash "$SRC/scripts/linux/build.sh" "$ROOT" "$@" || failed
exit 0
