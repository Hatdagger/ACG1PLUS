#!/usr/bin/env bash
# ACG1PLUS: start the Steam Deck / Linux game. The Linux twin of ACG1PLUS.exe
# (launcher/acg_starter.c); the build copies this file to the top of the game
# folder as "Start ACG1PLUS.sh".
#
#   bash "Start ACG1PLUS.sh"
#
# For Game Mode, add it to Steam in Desktop Mode (Add a Non-Steam Game, browse
# to this file); see docs/building.md.
#
# It makes sure "Memory Card" and "Mods" exist, starts the game in
# RecompData/linux/ with its memory cards pointed at "Memory Card" (shared
# with the Windows game), and hands back its exit code. Anything after the
# script name is passed on untouched; if that already names a memory card
# folder (--memcard-dir), it wins.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="$ROOT/RecompData/linux"
GAME="$GAME_DIR/ACG1PLUS_Game"

if [ ! -f "$GAME" ]; then
    echo "The Deck game is not built yet, or its files are missing. Run:  bash \"$ROOT/Build ACG1PLUS.sh\""
    echo "and keep the RecompData folder next to this file."
    exit 1
fi
mkdir -p "$ROOT/Memory Card" "$ROOT/Mods"
[ -x "$GAME" ] || chmod +x "$GAME" 2>/dev/null

cd "$GAME_DIR" || exit 1
case " $* " in
    *" --memcard-dir"*) exec "$GAME" "$@" ;;
    *) exec "$GAME" --memcard-dir "$ROOT/Memory Card" "$@" ;;
esac
