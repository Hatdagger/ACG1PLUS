<#
  ACG1PLUS: build the game on this Windows PC from your own disc.

  Normally started by "Build ACG1PLUS.bat" in the game folder, which first
  fetches this repository into RecompData\build\source\. See docs/building.md.

  Everything happens inside the game folder (portable install, owner decision
  2026-11-26). Nothing is installed, nothing is written to AppData or the
  registry, and the user PATH is checked before and after:

    <game folder>\
      ACG1PLUS.exe            start the game (copied here at the end)
      Build ACG1PLUS.bat      build / update
      Disc\                   your Project Phantasma USA .cue + .bin
      Memory Card\  Mods\     yours, never touched by a build
      RecompData\
        windows\              the game this script makes
        linux\                the Steam Deck game (ladder 0.1), left alone
        launcher\             the launcher's own small files, left alone
        build\
          source\             this repository + the framework
          tools-windows\      git (if this PC has none) and the compiler pack
          temp\  logs\        temporary files and build logs

  No game code is downloaded: the game is translated from your disc here.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameRoot,
    # Re-translate the game even if nothing changed (slow).
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

# ---- pinned downloads (versions + SHA-256, checked before use) --------------
# The compiler pack the framework pin expects (psxrecomp 44a401b7 asks for
# 1.0.14 or newer). One fixed version, so every build of this game is made by
# the same compiler (netplay needs identical builds).
$PackVersion = 'v1.0.14'
$PackUrl     = "https://github.com/RetroPortingToolKit/RetroPorting-Toolchains/releases/download/$PackVersion/cmake-clang-v1-windows-x64.zip"
$PackSha256  = '28da9742385e7ff875b3d9311e8ed89dbdc84f27b6ecba2bc0d0acc11f6d2b4d'
# Exact disc this build is for (game.toml [prepare_disc]).
$ExeName     = 'ACG1PLUS_Game'

# ---- folders ----------------------------------------------------------------
$GameRoot = (Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')
$Data   = Join-Path $GameRoot 'RecompData'
$Build  = Join-Path $Data 'build'
$Src    = Join-Path $Build 'source'
$Tools  = Join-Path $Build 'tools-windows'
$Pack   = Join-Path $Tools 'toolchain'
$Temp   = Join-Path $Build 'temp'
$Logs   = Join-Path $Build 'logs'
$Game   = Join-Path $Data 'windows'
foreach ($d in @($Data, $Build, $Tools, $Temp, $Logs, (Join-Path $GameRoot 'Disc'),
                 (Join-Path $GameRoot 'Memory Card'), (Join-Path $GameRoot 'Mods'))) {
    New-Item -ItemType Directory -Force -Path $d | Out-Null
}

$stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$LogFile = Join-Path $Logs "build-$stamp.txt"
Start-Transcript -LiteralPath $LogFile -Append | Out-Null

function Step([string]$text) { Write-Host ''; Write-Host "==> $text" -ForegroundColor Cyan }
function Fail([string]$text) {
    Write-Host ''
    Write-Host "BUILD STOPPED: $text" -ForegroundColor Red
    Write-Host "The full log is in $LogFile"
    Stop-Transcript | Out-Null
    exit 1
}

# ---- keep every tool inside the folder ----------------------------------------
# Temporary files, compiler caches and the framework's toolchain lookups all
# point here, for this window only.
$env:TEMP = $Temp
$env:TMP  = $Temp
$env:CCACHE_DIR = Join-Path $Tools 'ccache'
$env:RETCOMM_TOOLCHAIN_DIR = $Pack
$env:PSXRECOMP_TOOLCHAIN_DIR = $Pack
$env:RETCOMM_TOOLCHAIN_CACHE = Join-Path $Tools 'cache'
$env:RETCOMM_DATA_HOME = Join-Path $Tools 'retcomm'
$env:RETCOMM_TOOLCHAIN_SKIP_UPDATE = '1'
$env:PSXRECOMP_TOOLCHAIN_READONLY = '1'
$env:PYTHONNOUSERSITE = '1'
$env:PIP_NO_INDEX = '1'
$env:GIT_CONFIG_NOSYSTEM = '1'
# A folder on a USB drive (exFAT) has no owner; tell git that is fine, for this
# window only (the same three lines as in "Build ACG1PLUS.bat").
$env:GIT_CONFIG_COUNT = '1'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = '*'
# The framework's game-wide tools look for a project root here.
$env:PSXRECOMP_PROJECT_ROOT = $Src

# Watch the user PATH in the registry: the framework's toolchain installer can
# add itself there. This build never calls it; if anything did, put it back.
$UserPathBefore = [Environment]::GetEnvironmentVariable('Path', 'User')

function Check-UserPath {
    $after = [Environment]::GetEnvironmentVariable('Path', 'User')
    if ($after -ne $UserPathBefore) {
        Write-Host 'Something changed your user PATH during the build. Putting it back.' -ForegroundColor Yellow
        [Environment]::SetEnvironmentVariable('Path', $UserPathBefore, 'User')
    }
}

function Get-Sha256([string]$path) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
}

function Download([string]$url, [string]$dest, [string]$sha) {
    if ((Test-Path -LiteralPath $dest) -and ((Get-Sha256 $dest) -eq $sha)) { return }
    Write-Host "Downloading $url"
    & curl.exe -L --fail --retry 3 -o $dest $url
    if ($LASTEXITCODE -ne 0) { Fail "download failed: $url" }
    $got = Get-Sha256 $dest
    if ($got -ne $sha) {
        Remove-Item -LiteralPath $dest -Force
        Fail "download does not match its checksum: $url (got $got)"
    }
}

function Run([string]$what, [string]$exe, [string[]]$arguments) {
    Write-Host "$what"
    & $exe @arguments
    $code = $LASTEXITCODE
    Check-UserPath
    if ($code -ne 0) { Fail "$what (exit code $code)" }
}

# ---- 1. compiler pack (cmake, ninja, clang, Python) -------------------------
Step '1/6  Compiler pack'
$PackStamp = Join-Path $Pack '.acg-pack-version'
$havePack = (Test-Path (Join-Path $Pack 'bin\cmake.exe')) -and (Test-Path (Join-Path $Pack 'python\python.exe')) -and
            (Test-Path $PackStamp) -and ((Get-Content -LiteralPath $PackStamp -Raw).Trim() -eq $PackVersion)
if (-not $havePack) {
    $zip = Join-Path $Tools "cmake-clang-v1-windows-x64-$PackVersion.zip"
    Download $PackUrl $zip $PackSha256
    if (Test-Path -LiteralPath $Pack) { Remove-Item -LiteralPath $Pack -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Pack | Out-Null
    Write-Host 'Unpacking (about 800 MB)...'
    & tar.exe -xf $zip -C $Pack
    if ($LASTEXITCODE -ne 0) { Fail 'could not unpack the compiler pack' }
    Set-Content -LiteralPath $PackStamp -Value $PackVersion
    Remove-Item -LiteralPath $zip -Force
} else {
    Write-Host "Compiler pack $PackVersion is already here."
}
$Python = Join-Path $Pack 'python\python.exe'
$env:PATH = (Join-Path $Pack 'bin') + ';' + (Join-Path $Pack 'python') + ';' + $env:PATH

# git: the framework's patches are applied with it (CMakeLists.txt).
$git = Get-Command git.exe -ErrorAction SilentlyContinue
$portableGit = Join-Path $Tools 'git\cmd'
if (Test-Path (Join-Path $portableGit 'git.exe')) { $env:PATH = "$portableGit;" + $env:PATH }
elseif (-not $git) { Fail 'git was not found. Start the build with "Build ACG1PLUS.bat", which fetches it.' }

$Cli = Join-Path $Src 'psxrecomp\psxrecomp_cli.py'
$Toml = Join-Path $Src 'game.toml'
if (-not (Test-Path -LiteralPath $Cli)) { Fail "the source is incomplete: $Cli is missing" }

# ---- 2. your disc ---------------------------------------------------------------
Step '2/6  Your disc (Project Phantasma USA, SLUS-00670)'
$found = $null
$places = @((Join-Path $GameRoot 'Disc'), $GameRoot)
foreach ($place in $places) {
    foreach ($cue in (Get-ChildItem -LiteralPath $place -Filter '*.cue' -File -ErrorAction SilentlyContinue | Sort-Object Name)) {
        Write-Host "Checking $($cue.FullName)"
        & $Python $Cli verify-disc --config $Toml --disc $cue.FullName | Out-Host
        if ($LASTEXITCODE -eq 0) { $found = $cue.FullName; break }
    }
    if ($found) { break }
}
if (-not $found) {
    Fail ('no matching disc found. Put your Project Phantasma USA disc image (.cue + .bin, ' +
          'one data track, Redump size 454,072,416 bytes) in ' + (Join-Path $GameRoot 'Disc'))
}
Write-Host "Using $found" -ForegroundColor Green

# ---- 3. translate the game from the disc --------------------------------------
# The main program, then (aot/overlays.json) the code the game loads from the
# disc while playing. Both land in source\generated\, which never leaves this PC.
Step '3/6  Translating the game from your disc (10-30 minutes the first time)'
$genArgs = @($Cli, 'generate', '--config', $Toml, '--project-root', $Src, '--disc', $found)
if ($Force) { $genArgs += '--force-aot-static' }
Run 'psxrecomp generate' $Python $genArgs
# Generate copies the disc into source\disc\ the first time. Only the boot
# program it extracted is needed again (next builds read your Disc\ copy).
foreach ($copy in (Get-ChildItem -LiteralPath (Join-Path $Src 'disc') -File -ErrorAction SilentlyContinue)) {
    if ($copy.Extension -in @('.bin', '.cue') -and $copy.FullName -ne $found -and
        $copy.FullName -ne [IO.Path]::ChangeExtension($found, '.bin')) {
        Remove-Item -LiteralPath $copy.FullName -Force
    }
}

# ---- 4. compile -----------------------------------------------------------------
Step '4/6  Compiling (20-60 minutes the first time, faster after)'
Run 'psxrecomp rebuild' $Python @($Cli, 'rebuild', '--config', $Toml, '--project-root', $Src,
    '--build-dir', 'build-release', '--target', 'psx-runtime', '--exe-basename', $ExeName,
    '--no-toolchain-download', '--no-pgo',
    '--cmake-extra=-DPSX_SDL_BACKEND=SDL3', '--cmake-extra=-DPSX_PGXP_VARIANT=OFF',
    '--cmake-extra=-DPSX_RELEASE_CHANNEL=playtest')
$Out = Join-Path $Src 'build-release'
$Exe = Join-Path $Out "$ExeName.exe"
if (-not (Test-Path -LiteralPath $Exe)) { Fail "the build finished but $Exe is missing" }

# ---- 5. put the game into RecompData\windows ------------------------------------
Step '5/6  Putting the game in place'
Copy-Item -LiteralPath $Exe -Destination (New-Item -ItemType Directory -Force -Path $Game) -Force
Get-ChildItem -LiteralPath $Out -Filter '*.dll' -File | Copy-Item -Destination $Game -Force
foreach ($f in @('game.toml', 'game_options.toml', 'psx_game_version.txt')) {
    $p = Join-Path $Out $f
    if (Test-Path -LiteralPath $p) { Copy-Item -LiteralPath $p -Destination $Game -Force }
}
# Folders the runtime reads beside the exe. robocopy /MIR makes each one an
# exact copy of the build's (removing stale files); the player's own files
# (settings.toml, saves, caches, keyboard profiles) are outside them.
foreach ($dir in @('assets', 'bios', 'mods', 'overlay_toolchain')) {
    $from = Join-Path $Out $dir
    if (-not (Test-Path -LiteralPath $from)) { continue }
    $to = Join-Path $Game $dir
    # mods\installed holds the player's installed mods: never mirror over it.
    $keep = @()
    if ($dir -eq 'mods') { $keep = @('/XD', (Join-Path $to 'installed')) }
    & robocopy.exe $from $to /MIR /NFL /NDL /NJH /NJS /NP @keep | Out-Null
    if ($LASTEXITCODE -ge 8) { Fail "could not copy $dir" }
}
# Third-party notices (compiler runtime, SDL, OpenBIOS ...), from the framework.
$notices = Join-Path $Src 'psxrecomp\runtime\licenses'
if (Test-Path -LiteralPath $notices) {
    & robocopy.exe $notices (Join-Path $Game 'licenses') /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { Fail 'could not copy the licenses' }
}
$starter = Join-Path $Out 'starter\ACG1PLUS.exe'
if (Test-Path -LiteralPath $starter) { Copy-Item -LiteralPath $starter -Destination $GameRoot -Force }
Copy-Item -LiteralPath (Join-Path $Src 'Build ACG1PLUS.bat') -Destination $GameRoot -Force

# Portable: the background overlay compiler is named by a path relative to
# overlay_toolchain\ (the framework accepts that), so the folder can move.
$compilerTxt = Join-Path $Game 'overlay_toolchain\compiler.txt'
$clang = Join-Path $Pack 'bin\clang.exe'
if ((Test-Path -LiteralPath $compilerTxt) -and (Test-Path -LiteralPath $clang)) {
    $rel = '..\..\build\tools-windows\toolchain\bin\clang.exe'
    Set-Content -LiteralPath $compilerTxt -Value $rel -Encoding ASCII
    Write-Host "Background compiler: $rel (relative, so the folder can be moved)"
}

# ---- 6. check ----------------------------------------------------------------------
Step '6/6  Checking'
Check-UserPath
$problems = @()
if (-not (Test-Path (Join-Path $GameRoot 'ACG1PLUS.exe'))) { $problems += 'ACG1PLUS.exe is missing' }
if (-not (Test-Path (Join-Path $Game "$ExeName.exe"))) { $problems += "$ExeName.exe is missing" }
if (-not (Test-Path (Join-Path $Game 'bios\openbios.bin'))) { $problems += 'bios\openbios.bin is missing' }
$appdata = @((Join-Path $env:LOCALAPPDATA 'retcomm'), (Join-Path $env:LOCALAPPDATA 'psxrecomp'),
             (Join-Path $env:APPDATA 'ACG1PLUS'))
foreach ($a in $appdata) {
    if (Test-Path -LiteralPath $a) { Write-Host "Note: $a exists (from another program or an older build; this build did not use it)." -ForegroundColor Yellow }
}
if ($problems.Count) { Fail ($problems -join '; ') }

$size = (Get-ChildItem -LiteralPath $GameRoot -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
Write-Host ''
Write-Host ('Done. Start the game with ' + (Join-Path $GameRoot 'ACG1PLUS.exe')) -ForegroundColor Green
Write-Host ('Game folder size: {0:N1} GB' -f ($size / 1GB))
Write-Host "Log: $LogFile"
Stop-Transcript | Out-Null
exit 0
