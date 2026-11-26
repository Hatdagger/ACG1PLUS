#!/usr/bin/env bash
# Explicit prebuilt-only wrapper for committed-C, no-disc CI (including macOS).
# It does not run selective PGO training. Official optimized Windows/Linux
# releases use package_release.sh and the maintained shared release builder.
# Autofilled by tools/new_project_layout/setup_project.{sh,ps1}.
#
# Ships the COMPILED game built from this repo's committed generated/ C:
# executable, runtime data, bundled OpenBIOS, mod catalog, overlay toolchain.
# No sources, emitters, CLI, generated C, or BIOS dumps.
#
# Usage:
#   scripts/package_prebuilt_release.sh <build-dir> <artifact-tag> [recompiler-build-dir]
#
# Writes: dist/acg-<VERSION>-<artifact-tag>.zip
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-}"
ARTIFACT_TAG="${2:-}"
RECOMPILER_BUILD="${3:-build-recompiler}"

if [[ -z "${BUILD_DIR}" || -z "${ARTIFACT_TAG}" ]]; then
  echo "usage: $0 <build-dir> <artifact-tag> [recompiler-build-dir]" >&2
  exit 2
fi

PACKAGER="${ROOT}/psxrecomp/tools/package_game_release.sh"
if [[ ! -f "${PACKAGER}" ]]; then
  echo "error: missing ${PACKAGER} (psxrecomp submodule predates bundled releases -- bump it)" >&2
  exit 1
fi
chmod +x "${PACKAGER}" 2>/dev/null || true

EXTRA=()
# Overlay shard cache. It is compiled FROM THE DISC, so CI (no disc) cannot
# build one: the committed static AOT shard is linked into the executable and
# the bundled overlay_toolchain/ compiles the rest from the player's disc at
# runtime. A developer packaging locally with a cache can ship it instead:
#   PSX_OVERLAY_CACHE_ROOT=/path/to/cache scripts/package_prebuilt_release.sh ...
if [[ -n "${PSX_OVERLAY_CACHE_ROOT:-}" ]]; then
  EXTRA+=(--overlay-cache-root "${PSX_OVERLAY_CACHE_ROOT}")
else
  EXTRA+=(--ship-without-overlay-cache-because \
    "no disc at package time: the committed static AOT shard is compiled in and overlay_toolchain/ autocompiles the rest on the player's machine")
fi
# Extra docs shipped at the zip root (DISC.md tells players which dump works).
for _doc in DISC.md; do
  if [[ -f "${ROOT}/${_doc}" ]]; then
    EXTRA+=(--doc "${_doc}")
  fi
done

cd "${ROOT}"
package() {   # <build-dir> <artifact-tag> [extra packager args...]
  local build="$1" tag="$2"; shift 2
  bash "${PACKAGER}" \
    --root "${ROOT}" \
    --build-dir "${build}" \
    --artifact "${tag}" \
    --zip-prefix acg \
    --exe-name Armored_Core_Gen_1 \
    --display-name "Armored Core Gen 1" \
    --recompiler-build "${RECOMPILER_BUILD}" \
    --version-env RELEASE_VERSION \
    --disc-hint "your legally owned Armored Core Gen 1 disc" \
    "${EXTRA[@]}" "$@"
}
package "${BUILD_DIR}" "${ARTIFACT_TAG}"

# The Japanese disc's program (CMakeLists.txt ACG_REGION=JP), built in its own
# build folder: ACG_JP_BUILD_DIR=<dir>. Windows only for now: the framework's
# switch between the two programs (regional_launch.h) starts the other program
# on Windows only.
JP_STAGE=""
if [[ -n "${ACG_JP_BUILD_DIR:-}" && "${ARTIFACT_TAG}" == windows-* ]]; then
  package "${ACG_JP_BUILD_DIR}" "${ARTIFACT_TAG}-jp" --stage-only
  JP_STAGE="${ROOT}/dist/stage-game-${ARTIFACT_TAG}-jp"
  [[ -d "${JP_STAGE}" ]] || { echo "error: Japanese stage ${JP_STAGE} missing" >&2; exit 1; }
  grep -q '^id = "SLPS-01855"' "${JP_STAGE}/game.toml" \
    || { echo "error: the Japanese stage's game.toml is not the SLPS-01855 one" >&2; exit 1; }
fi

# --- Windows: the player's folder layout ----------------------------------------
#   Armored Core Gen 1.exe   starts the game (launcher/acg_starter.c)
#   Memory Card/             memory cards
#   Mods/                    drop folder for mod packages
#   RecompData/              everything the framework's packager staged
# The framework packages a flat folder; this moves its stage into RecompData/,
# adds the starter and the two player folders, and writes the zip again under
# the same name. Linux and macOS keep the flat folder for now.
if [[ "${ARTIFACT_TAG}" == windows-* ]]; then
  DIST="${ROOT}/dist"
  STAGE="${DIST}/stage-game-${ARTIFACT_TAG}"
  ZIP="$(ls -t "${DIST}"/acg-*-"${ARTIFACT_TAG}".zip | head -1)"
  STARTER="${BUILD_DIR}/starter/Armored Core Gen 1.exe"
  [[ -d "${STAGE}" ]] || { echo "error: packager stage ${STAGE} is gone" >&2; exit 1; }
  [[ -f "${ZIP}" ]] || { echo "error: no ${ARTIFACT_TAG} zip in ${DIST}" >&2; exit 1; }
  [[ -f "${STARTER}" ]] || { echo "error: starter not built: ${STARTER}" >&2; exit 1; }
  LAYOUT="${DIST}/layout-${ARTIFACT_TAG}"
  rm -rf "${LAYOUT}"
  mkdir -p "${LAYOUT}/Memory Card" "${LAYOUT}/Mods"
  mv "${STAGE}" "${LAYOUT}/RecompData"
  cp "${STARTER}" "${LAYOUT}/"
  # Both disc versions: the Japanese program in RecompData/Japan/ (its own
  # game.toml and player files), and the framework's list of which program
  # plays which disc. Discs are known by the SHA-256 and size of the image's
  # data track (regional/README.md).
  if [[ -n "${JP_STAGE}" ]]; then
    # The exe names CMake chose (the framework records them in the build folder).
    US_EXE="$(tr -d '[:space:]' <"${BUILD_DIR}/psxrecomp_exe_name-psx-runtime.txt").exe"
    JP_EXE="$(tr -d '[:space:]' <"${ACG_JP_BUILD_DIR}/psxrecomp_exe_name-psx-runtime.txt").exe"
    [[ -f "${LAYOUT}/RecompData/${US_EXE}" && -f "${JP_STAGE}/${JP_EXE}" ]] \
      || { echo "error: game exe not found: RecompData/${US_EXE} or ${JP_STAGE}/${JP_EXE}" >&2; exit 1; }
    for doc in README.txt START_HERE.txt DISC.md; do rm -f "${JP_STAGE}/${doc}"; done
    mv "${JP_STAGE}" "${LAYOUT}/RecompData/Japan"
    sed -e "s|@US_EXE@|${US_EXE}|" -e "s|@JP_EXE@|Japan/${JP_EXE}|" \
      "${ROOT}/regional/regional-launch.toml.in" >"${LAYOUT}/RecompData/regional-launch.toml"
    echo "Added the Japanese program: RecompData/Japan/${JP_EXE} (US program: RecompData/${US_EXE})"
  fi
  # The starter is all the player needs to find at the top of the folder.
  for doc in README.txt START_HERE.txt DISC.md; do
    if [[ -f "${LAYOUT}/RecompData/${doc}" ]]; then
      mv "${LAYOUT}/RecompData/${doc}" "${LAYOUT}/${doc}"
    fi
  done
  cat >"${LAYOUT}/README.txt" <<EOF_README
Armored Core Gen 1 ${RELEASE_VERSION:-}

1. Start "Armored Core Gen 1.exe".
2. Put your own Master of Arena disc 1 image (.cue + .bin) in this folder or in
   a "disc" folder here. The game finds it by itself. US disc (SLUS-01030) and
   Japanese disc (SLPS-01855) both work; the right version starts by itself.

Memory Card   your memory cards (card1.mcd, card2.mcd). Back these up.
Mods          put new mod packages (.psxmod) here, then install them from MODS.
RecompData    the game itself. Leave it alone; updates replace it.

Your memory cards, mods, discs and settings are never touched by an update.
Third-party licences are in RecompData/licenses.
EOF_README
  cat >"${LAYOUT}/Memory Card/About this folder.txt" <<'EOF_CARDS'
Your memory cards live here: card1.mcd (slot 1) and card2.mcd (slot 2).
A card from an emulator can be used too: name it card1.mcd and put it here
(keep a copy of the one that was here first).
EOF_CARDS
  cat >"${LAYOUT}/Mods/About this folder.txt" <<'EOF_MODS'
Put new mod packages (.psxmod files) here, then open MODS in the game's menu
to install them. An installed package is renamed to end in .installed.
EOF_MODS
  find "${LAYOUT}" -exec touch -c {} + 2>/dev/null || true
  rm -f "${ZIP}"
  PY="$(command -v python3 || command -v python)"
  "${PY}" "${ROOT}/psxrecomp/tools/create_release_zip.py" --source "${LAYOUT}" --output "${ZIP}" >/dev/null
  echo "Rewrote ${ZIP} with the player's folder layout (RecompData/)"
fi
