@echo off
rem ============================================================================
rem  ACG1PLUS - build (or update) the game on this PC from your own disc.
rem
rem  First time: put this file in an empty folder (it becomes the game folder,
rem  for example E:\Games\PC\Recomps\ACG1PLUS), put your Project Phantasma USA
rem  .cue + .bin in a "Disc" folder beside it, and double-click this file.
rem  Later: double-click it again to update and rebuild.
rem
rem  Everything stays in this folder (portable): nothing is installed, nothing
rem  goes to AppData. It downloads only tools (git if this PC has none, the
rem  compiler pack) and this project's source. No game code is downloaded: the
rem  game is translated from your disc here. See docs/building.md.
rem ============================================================================
setlocal EnableExtensions

rem Which version of the project to build. Change to main once build/pp-0.0 is merged.
set "ACG_BRANCH=build/pp-0.0"
set "ACG_REPO=https://github.com/Hatdagger/ACG1PLUS.git"

rem PortableGit, used only when this PC has no git. Pinned and checked.
set "PG_VERSION=2.56.1"
set "PG_URL=https://github.com/git-for-windows/git/releases/download/v2.56.1.windows.1/PortableGit-2.56.1-64-bit.7z.exe"
set "PG_SHA256=9dd04a608ceb5f6ab32a73709e0ecb159d211da71b697c45ff81f6dd0c6b49c7"

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD=%ROOT%\RecompData\build"
set "SRC=%BUILD%\source"
set "TOOLS=%BUILD%\tools-windows"
set "TEMP=%BUILD%\temp"
set "TMP=%BUILD%\temp"
rem A folder on a USB drive (exFAT) has no owner; tell git that is fine, for
rem this window only (nothing is written to git's settings).
set "GIT_CONFIG_COUNT=1"
set "GIT_CONFIG_KEY_0=safe.directory"
set "GIT_CONFIG_VALUE_0=*"
set "GIT_CONFIG_NOSYSTEM=1"
if not exist "%TOOLS%" mkdir "%TOOLS%"
if not exist "%TEMP%" mkdir "%TEMP%"
if not exist "%ROOT%\Disc" mkdir "%ROOT%\Disc"

echo.
echo ACG1PLUS build - game folder: %ROOT%
echo.

rem ---- git ----------------------------------------------------------------
set "GIT="
if exist "%TOOLS%\git\cmd\git.exe" set "GIT=%TOOLS%\git\cmd\git.exe"
if not defined GIT (
  where git.exe >nul 2>nul && set "GIT=git.exe"
)
if not defined GIT (
  echo ==^> Fetching PortableGit %PG_VERSION% into RecompData\build\tools-windows\git
  powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference='Stop'; $f=Join-Path $env:TOOLS 'PortableGit.7z.exe';" ^
    "& curl.exe -L --fail --retry 3 -o $f $env:PG_URL; if ($LASTEXITCODE) { exit 1 };" ^
    "if ((Get-FileHash -Algorithm SHA256 -LiteralPath $f).Hash.ToLower() -ne $env:PG_SHA256) { Remove-Item $f; Write-Host 'PortableGit checksum does not match'; exit 1 }"
  if errorlevel 1 goto :failed
  "%TOOLS%\PortableGit.7z.exe" -o"%TOOLS%\git" -y
  if errorlevel 1 goto :failed
  del "%TOOLS%\PortableGit.7z.exe"
  set "GIT=%TOOLS%\git\cmd\git.exe"
)

rem ---- source (this project + the framework, no game code) ------------------
if not exist "%SRC%\.git" (
  echo ==^> Fetching the source ^(%ACG_BRANCH%^)
  "%GIT%" clone --depth 1 --branch "%ACG_BRANCH%" "%ACG_REPO%" "%SRC%"
  if errorlevel 1 goto :failed
) else (
  echo ==^> Updating the source ^(%ACG_BRANCH%^)
  "%GIT%" -C "%SRC%" fetch --depth 1 origin "%ACG_BRANCH%"
  if errorlevel 1 goto :failed
  "%GIT%" -C "%SRC%" reset --hard FETCH_HEAD
  if errorlevel 1 goto :failed
)
rem --force: the build re-applies our small framework patches itself.
"%GIT%" -C "%SRC%" submodule update --init --recursive --depth 1 --force
if errorlevel 1 goto :failed

rem ---- the build -------------------------------------------------------------
powershell -NoProfile -ExecutionPolicy Bypass -File "%SRC%\scripts\windows\build.ps1" -GameRoot "%ROOT%" %*
if errorlevel 1 goto :failed
echo.
pause
exit /b 0

:failed
echo.
echo The build stopped. The log is in RecompData\build\logs\ (if it got that far).
pause
exit /b 1
