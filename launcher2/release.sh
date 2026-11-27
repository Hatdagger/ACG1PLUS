#!/usr/bin/env bash
# Makes every file of a launcher release (test or real) on a Linux machine. Called by
# .github/workflows/launcher-test-release.yml; can be run by hand the same way.
#
#   launcher2/release.sh WORK_DIR TAG [REPO]
#
#   TAG   the release the files go on, for example test-launcher-0.2.2
#   REPO  owner/name on GitHub (default Hatdagger/ACG1PLUS)
#
# Leaves in WORK_DIR/files/:
#   ACG1PLUS                          Steam Deck / Linux launcher
#   ACG1PLUS.exe                      Windows launcher
#   acg1plus-source-<version>.zip     our code + the framework at its pins + the library
#                                     sources, framework patches applied; no game code
#                                     (tools/release/make_source_zip.py)
#   manifest.json                     version, tag, every file's SHA-256 and size, the
#                                     compiler packs the launcher downloads
#   SHA256SUMS.txt
#
# Order matters: the source zip is made first, and its download address, SHA-256 and
# size are built into both launchers, so each launcher builds exactly that source and
# checks it before use. Re-running for the same tag makes a new zip (new checksum)
# and new launchers to match; a launcher downloaded before then no longer finds its
# zip and must be downloaded again.
set -euo pipefail

WORK="${1:?usage: launcher2/release.sh WORK_DIR TAG [REPO]}"
TAG="${2:?usage: launcher2/release.sh WORK_DIR TAG [REPO]}"
REPO="${3:-Hatdagger/ACG1PLUS}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"
FILES="$WORK/files"
rm -rf "$FILES"
mkdir -p "$FILES"

VERSION="$(tr -d ' \r\n' < "$HERE/VERSION")"

# The framework and its own submodules at their pins (the source zip needs them all).
git -C "$ROOT" submodule update --init --recursive --depth 1

# 1. The source zip (refuses to finish if game code or disc files are in the tree).
# ACG_ZIP_ARGS: extra options for the zip script, sandbox tests only (e.g.
# "--allow-missing psx_libwebp psx_openxr_sdk", where codeload.github.com is blocked).
# shellcheck disable=SC2086
python3 "$ROOT/tools/release/make_source_zip.py" --version "$VERSION" --out "$FILES" ${ACG_ZIP_ARGS:-}
ZIP="$FILES/acg1plus-source-$VERSION.zip"
export ACG_SOURCE_VERSION="$VERSION"
export ACG_SOURCE_SHA256="$(tr -d ' \r\n' < "$FILES/acg1plus-source-$VERSION.sha256")"
export ACG_SOURCE_SIZE="$(stat -c %s "$ZIP")"
export ACG_SOURCE_URL="https://github.com/$REPO/releases/download/$TAG/acg1plus-source-$VERSION.zip"
export ACG_REPO="$REPO"
rm -f "$FILES/acg1plus-source-$VERSION.sha256"
echo "Source zip: $ACG_SOURCE_URL ($ACG_SOURCE_SIZE bytes, sha256 $ACG_SOURCE_SHA256)"

# 2. Both launchers, with that source built in.
"$HERE/build.sh" "$WORK" both
cp "$WORK/out/linux/ACG1PLUS" "$WORK/out/windows/ACG1PLUS.exe" "$FILES/"
chmod +x "$FILES/ACG1PLUS"

# 3. manifest.json and SHA256SUMS.txt (the launcher's UPDATES read the manifest from 0.2.4:
#    launcher_version, summary from WHATSNEW.txt, and each launcher's url + sha256 + size).
python3 - "$FILES" "$VERSION" "$TAG" "$REPO" "$(git -C "$ROOT" rev-parse HEAD)" "$HERE/WHATSNEW.txt" <<'PY'
import hashlib, json, os, sys
files, version, tag, repo, commit, whatsnew = sys.argv[1:7]
# One line the launcher shows for this release on its main menu and UPDATES page.
summary = ""
if os.path.exists(whatsnew):
    with open(whatsnew, encoding="utf-8") as f:
        summary = next((l.strip() for l in f if l.strip() and not l.startswith("#")), "")
def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()
entries = {}
for name in sorted(os.listdir(files)):
    p = os.path.join(files, name)
    entries[name] = {"sha256": sha(p), "size": os.path.getsize(p),
                     "url": f"https://github.com/{repo}/releases/download/{tag}/{name}"}
base = "https://github.com/RetroPortingToolKit/RetroPorting-Toolchains/releases/download/v1.0.14/"
manifest = {
    "name": "ACG1PLUS", "launcher_version": version, "tag": tag, "commit": commit,
    "summary": summary,
    "files": entries,
    "source_zip": f"acg1plus-source-{version}.zip",
    "compiler_packs": {
        "linux": {"url": base + "cmake-clang-v1-linux-x64.zip", "size": 833435449,
                  "sha256": "597c8d343a3cf02ba6f6b2ae7cf6fe2fef125dde8feff62a144e8dd3da3d484e"},
        "windows": {"url": base + "cmake-clang-v1-windows-x64.zip", "size": 209497009,
                    "sha256": "28da9742385e7ff875b3d9311e8ed89dbdc84f27b6ecba2bc0d0acc11f6d2b4d"},
    },
}
with open(os.path.join(files, "manifest.json"), "w") as f:
    json.dump(manifest, f, indent=2)
    f.write("\n")
PY
(cd "$FILES" && sha256sum ACG1PLUS ACG1PLUS.exe "acg1plus-source-$VERSION.zip" manifest.json > SHA256SUMS.txt)
cat "$FILES/SHA256SUMS.txt"
echo "Files for $TAG: $FILES"
