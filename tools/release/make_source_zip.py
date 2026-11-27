#!/usr/bin/env python3
"""Make acg1plus-source-<version>.zip: everything a player's PC needs to build
the game from their own disc, with no git and no downloads during the build.

    python3 tools/release/make_source_zip.py --version 0.2.2 --out DIR
        [--deps-from DIR] [--allow-missing NAME ...]

What goes in (launcher plan, owner decision 2026-11-26):
- this repository at HEAD (tracked files only),
- the framework (psxrecomp, recomp-ui and their own submodules) at the pinned
  commits, with our small patches (patches/) already applied, listed in
  patches/APPLIED.txt so CMake doesn't look for git,
- the library sources the build would otherwise download
  (psxrecomp/third_party/, from its deps.manifest, checked against the pinned
  SHA-256), so the launcher can build with PSX_DEPS_OFFLINE=1.

No game code: the same names the workflow refuses are refused here, in the
finished tree, before the zip is written (docs/no-game-code.md). The repo's
.github/ folder is left out (players don't need it).

Writes DIR/acg1plus-source-<version>.zip and DIR/acg1plus-source-<version>.sha256
(the hex digest alone). Run from a checkout whose submodules are initialised
(git submodule update --init --recursive).

--deps-from DIR: take dependency archives from DIR instead of downloading
(still checked against the manifest). --allow-missing NAME: leave that
dependency out (sandbox only, where codeload.github.com is blocked); the zip
then says offline=0 in SOURCE.txt and the build may download it.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Paths that must never be in the zip, for this repo's own files (same idea as
# the workflow's check). Matched against the path inside the zip.
FORBIDDEN = re.compile(
    r"(^|/)generated(-jp)?/"                       # translated game code
    r"|overlays_static.*\.c$"
    r"|(^|/)(SLUS|SLPS|SCUS|SLES|SCES)_[0-9]"      # boot executables
    r"|(^|/)SYSTEM\.CNF$"
    r"|\.(bin|cue|iso|chd|img|mdf|ccd|sub|ecm|pbp|mcd|mcr)$"
    r"|^\.cache/"
    r"|(^|/)build-release/",
    re.IGNORECASE,
)
# The framework (psxrecomp/, recomp-ui/) is the upstream project at its pinned
# commit; it tracks its own recompiled BIOS, generated headers and a homebrew
# test disc and memory-card test files. Its files are checked only for disc images and boot programs, with
# these known framework files allowed by exact name.
FRAMEWORK_PREFIXES = ("psxrecomp/", "recomp-ui/")
FRAMEWORK_FORBIDDEN = re.compile(
    r"(^|/)(SLUS|SLPS|SCUS|SLES|SCES)_[0-9]"
    r"|(^|/)SYSTEM\.CNF$"
    r"|\.(cue|iso|chd|img|mdf|ccd|sub|ecm|pbp)$",
    re.IGNORECASE,
)
FRAMEWORK_ALLOWED = {
    "psxrecomp/tools/cycle_testrom/disc/SYSTEM.CNF",   # the framework's own homebrew test ROM
}


def run(cmd, cwd=None, **kw):
    return subprocess.run(cmd, cwd=cwd, check=True, text=True, capture_output=True, **kw).stdout


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def tracked_files() -> list[str]:
    # Every tracked file of this repo and of every (nested) submodule.
    out = run(["git", "ls-files", "-z", "--recurse-submodules"], cwd=ROOT)
    return [p for p in out.split("\0") if p]


def check_submodules() -> dict[str, str]:
    pins = {}
    for line in run(["git", "submodule", "status", "--recursive"], cwd=ROOT).splitlines():
        flag, rest = line[0], line[1:].split()
        if flag in "-+U":
            sys.exit(f"submodule not at its pinned commit (or not checked out): {line.strip()}\n"
                     "run: git submodule update --init --recursive")
        pins[rest[1]] = rest[0]
    return pins


def stage(dest: Path) -> int:
    count = 0
    for rel in tracked_files():
        if rel.startswith(".github/"):
            continue
        src = ROOT / rel
        if src.is_symlink() or not src.is_file():
            continue
        target = dest / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, target)
        count += 1
    return count


def apply_patches(dest: Path) -> list[str]:
    applied = []
    for sub in ("recomp-ui", "psxrecomp"):
        for patch in sorted((dest / "patches" / sub).glob("*.patch")):
            # git apply works outside a repository (plain patch mode).
            subprocess.run(["git", "apply", "--check", str(patch)], cwd=dest / sub, check=True)
            subprocess.run(["git", "apply", str(patch)], cwd=dest / sub, check=True)
            applied.append(f"{sub}/{patch.name}")
    (dest / "patches" / "APPLIED.txt").write_text(
        "These patches are already applied to the framework in this source zip\n"
        "(tools/release/make_source_zip.py), so the build does not need git.\n\n"
        + "".join(f"{a}\n" for a in applied))
    return applied


def vendor_deps(dest: Path, deps_from: Path | None, allow_missing: set[str]) -> tuple[list[str], list[str]]:
    tp = dest / "psxrecomp" / "third_party"
    have, missing = [], []
    for line in (tp / "deps.manifest").read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, file, sha, url = line.split()
        target = tp / file
        if target.is_file() and sha256_file(target) == sha:
            have.append(name)
            continue
        if name in allow_missing:
            missing.append(name)
            continue
        if deps_from and (deps_from / file).is_file():
            shutil.copy2(deps_from / file, target)
        else:
            print(f"downloading {url}", file=sys.stderr)
            with urllib.request.urlopen(url, timeout=120) as r, open(target, "wb") as f:
                shutil.copyfileobj(r, f)
        got = sha256_file(target)
        if got != sha:
            target.unlink()
            sys.exit(f"{name}: {file} does not match its pinned checksum (got {got})")
        have.append(name)
    return have, missing


def scan(dest: Path) -> None:
    bad = []
    for p in dest.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(dest).as_posix()
        if rel.startswith(FRAMEWORK_PREFIXES):
            if rel not in FRAMEWORK_ALLOWED and FRAMEWORK_FORBIDDEN.search(rel):
                bad.append(rel)
            continue
        if FORBIDDEN.search(rel):
            bad.append(rel)
        elif p.stat().st_size > 40 * 1024 * 1024:
            bad.append(rel + " (unexpectedly large)")
    if bad:
        sys.exit("Game code or disc files found, stopping:\n  " + "\n  ".join(sorted(bad)))


def write_zip(dest: Path, zip_path: Path, when: tuple) -> int:
    n = 0
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(dest.rglob("*")):
            if not p.is_file():
                continue
            rel = p.relative_to(dest).as_posix()
            info = zipfile.ZipInfo(rel, date_time=when)
            mode = p.stat().st_mode
            info.external_attr = ((0o755 if mode & stat.S_IXUSR else 0o644) | stat.S_IFREG) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(p, "rb") as f:
                z.writestr(info, f.read(), compresslevel=9)
            n += 1
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--deps-from", default="")
    ap.add_argument("--allow-missing", nargs="*", default=[])
    args = ap.parse_args()

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    pins = check_submodules()
    commit = run(["git", "rev-parse", "HEAD"], cwd=ROOT).strip()
    commit_time = int(run(["git", "show", "-s", "--format=%ct", "HEAD"], cwd=ROOT).strip())
    # Zip times = the commit time, never in the future (the framework clamps
    # future times, but there is no reason to make it).
    when = time.gmtime(min(commit_time, int(time.time())))[:6]

    with tempfile.TemporaryDirectory(prefix="acg-src-") as tmp:
        dest = Path(tmp) / "src"
        files = stage(dest)
        applied = apply_patches(dest)
        have, missing = vendor_deps(dest, Path(args.deps_from) if args.deps_from else None,
                                    set(args.allow_missing))
        (dest / "SOURCE.txt").write_text(
            f"name=ACG1PLUS\nversion={args.version}\ncommit={commit}\n"
            + "".join(f"pin.{k}={v}\n" for k, v in sorted(pins.items()))
            + f"offline={0 if missing else 1}\n"
            + f"deps={','.join(have)}\n"
            + (f"missing={','.join(missing)}\n" if missing else ""))
        scan(dest)
        zip_path = out / f"acg1plus-source-{args.version}.zip"
        n = write_zip(dest, zip_path, when)

    digest = sha256_file(zip_path)
    (out / f"acg1plus-source-{args.version}.sha256").write_text(digest + "\n")
    print(f"{zip_path.name}: {n} files ({files} from the repo and framework), "
          f"{zip_path.stat().st_size / 1e6:.1f} MB, patches {applied}, deps {have}"
          + (f", LEFT OUT {missing}" if missing else ""))
    print(f"sha256 {digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
