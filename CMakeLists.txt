cmake_minimum_required(VERSION 3.20)
set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH
    "Path to psxrecomp")
if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/runtime.cmake")
    message(FATAL_ERROR
        "PSXRecomp runtime is missing: ${PSXRECOMP_ROOT}/runtime/runtime.cmake\n"
        "This project expects the psxrecomp framework checkout at ${PSXRECOMP_ROOT}.\n"
        "Run: git submodule update --init --recursive\n"
        "If psxrecomp/ is not a submodule, set -DPSXRECOMP_ROOT=<path-to-psxrecomp>.")
endif()

project(Armored_Core_Gen_1 LANGUAGES C CXX)

# --- Opt-in product surfaces (OFF by default in the framework) -------------
# Scaffold prompts / flags fill these. Without recomp-ui, PSX_RECOMP_UI is OFF.
# The wizard (WIZARD_CMAKE_BLOCK / ENABLE_SETUP_WIZARD) is the first-run disc
# picker; releases are compiled from the committed generated/ C, so it never
# needs to generate or rebuild anything on a player's machine.
# recomp-ui submodule present (PSX_RECOMP_UI defaults ON).
if(EXISTS "${PSXRECOMP_ROOT}/lib/recomp-net/CMakeLists.txt")
    set(PSX_NETPLAY ON CACHE BOOL
        "Link recomp-net delay-sync (opt-in; needs recomp-net)" FORCE)
endif()
set(PSX_SETUP_WIZARD ON CACHE BOOL
    "Advertise first-run setup wizard + Generate & rebuild in recomp-ui" FORCE)

# --- Launcher front end (launcher/) ------------------------------------------
# recomp-ui has no hook for a title's own menu, so a small patch adds one
# (patches/recomp-ui/). It is applied to the submodule's working tree here and
# left alone when already applied; a recomp-ui bump that breaks it stops the
# configure so the patch gets refreshed.
set(ACG_RECOMP_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui")
file(GLOB ACG_RECOMP_UI_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/patches/recomp-ui/*.patch")
list(SORT ACG_RECOMP_UI_PATCHES)
find_package(Git QUIET)
if(ACG_RECOMP_UI_PATCHES AND NOT GIT_FOUND)
    message(FATAL_ERROR "git is needed to apply the recomp-ui patches in patches/recomp-ui")
endif()
foreach(_acg_patch IN LISTS ACG_RECOMP_UI_PATCHES)
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${_acg_patch}"
        WORKING_DIRECTORY "${ACG_RECOMP_UI_DIR}" RESULT_VARIABLE _acg_applied
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT _acg_applied EQUAL 0)
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${_acg_patch}"
            WORKING_DIRECTORY "${ACG_RECOMP_UI_DIR}" RESULT_VARIABLE _acg_apply
            ERROR_VARIABLE _acg_apply_err)
        if(NOT _acg_apply EQUAL 0)
            message(FATAL_ERROR "recomp-ui patch does not apply: ${_acg_patch}\n${_acg_apply_err}")
        endif()
        message(STATUS "Applied recomp-ui patch ${_acg_patch}")
    endif()
endforeach()

include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

# This project's own mod catalog. The FRAMEWORK stages it -- together with its
# builtin psx.* packages -- into <exe-dir>/mods/bundled, and refuses to
# configure if a catalog exists here but is not declared. Shared Internal
# Resolution is included even with a builtin allowlist, enabled at 4x on fresh
# profiles, with 1x..32x choices; do not supply a title-local copy of that id.
# Never copy the catalog by hand: five titles did, and framework 4cc04be3's rename of the staged
# directory broke all five silently (bead beads-eio.3.101).
set(PSX_PRELOADED_MODS "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded")
if(NOT IS_DIRECTORY "${PSX_PRELOADED_MODS}")
    set(PSX_PRELOADED_MODS "NONE")
endif()

# --- Which disc this build is for --------------------------------------------
# Each disc version is its own program (its code differs), built from its own
# committed game C:
#   US  Master of Arena USA disc 1, SLUS-01030: game.toml, generated/
#   JP  Master of Arena Japan disc 1, SLPS-01855: game-jp.toml, generated-jp/
# The Windows release ships both: the US program in RecompData/ and the
# Japanese one in RecompData/Japan/, each beside its own game.toml. The
# framework's regional switch (regional/regional-launch.toml, psxrecomp
# runtime/include/regional_launch.h) starts the program that matches the disc.
set(ACG_REGION "US" CACHE STRING "Disc this build is for: US (SLUS-01030) or JP (SLPS-01855)")
set_property(CACHE ACG_REGION PROPERTY STRINGS US JP)
if(ACG_REGION STREQUAL "JP")
    set(ACG_GEN_MARKER "generated-jp/SLPS_018.55_dispatch.c")
    set(ACG_GEN_FULL_GLOB "generated-jp/SLPS_018.55_full_*.c")
    set(ACG_GAME_TOML "game-jp.toml")
    set(ACG_REGION_JP 1)
elseif(ACG_REGION STREQUAL "US")
    set(ACG_GEN_MARKER "generated/SLUS_010.30_dispatch.c")
    set(ACG_GEN_FULL_GLOB "generated/SLUS_010.30_full_*.c")
    set(ACG_GAME_TOML "game.toml")
    set(ACG_REGION_JP 0)
else()
    message(FATAL_ERROR "ACG_REGION must be US or JP, not '${ACG_REGION}'")
endif()
message(STATUS "Armored Core Gen 1: building the ${ACG_REGION} disc program (${ACG_GAME_TOML})")

psxrecomp_add_game_runtime(psx-runtime
    PRELOADED_MODS_DIR "${PSX_PRELOADED_MODS}"
    ENABLE_NETPLAY_IF_PRESENT
    NETPLAY_LOBBY_URL "ws://netplay.retcomm.net:8765"
    ENABLE_SETUP_WIZARD
    WINDOW_TITLE "Armored Core: Master of Arena"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 2
    # LAUNCHER_BOXART "${CMAKE_CURRENT_SOURCE_DIR}/launcher_assets/img/boxart.tga"
    APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/psxrecomp.ico"
    # Committed game C (psxrecomp_cli.py generate; git add generated). Release
    # CI configures with -DPSXRECOMP_REQUIRE_GAME_C=ON so a checkout without
    # it fails instead of building a setup host.
    GEN_MARKER "${ACG_GEN_MARKER}"
    GEN_FULL_GLOB "${ACG_GEN_FULL_GLOB}"
    CODEGEN_SETUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/codegen_setup.c"
)

# Stage runtime configs next to the exe for direct build/ launches. Only copy
# files that exist — never fail POST_BUILD when game_options.toml is absent.
# The JP program's game-jp.toml is staged as game.toml: each program reads the
# game.toml beside it (and the packager ships that staged copy).
foreach(_PSX_RUNTIME_CONFIG game.toml game_options.toml)
    set(_acg_config_src "${_PSX_RUNTIME_CONFIG}")
    if(_PSX_RUNTIME_CONFIG STREQUAL "game.toml")
        set(_acg_config_src "${ACG_GAME_TOML}")
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_acg_config_src}")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${_acg_config_src}"
                "$<TARGET_FILE_DIR:psx-runtime>/${_PSX_RUNTIME_CONFIG}"
            COMMENT "Staging ${_acg_config_src} as ${_PSX_RUNTIME_CONFIG}"
            VERBATIM)
    endif()
endforeach()

# The launcher's own menu (launcher/moa_menu.inc, drawn inside recomp-ui's
# launcher) and its updater. The dev channel (Actions builds with a GitHub
# token) is compiled out of public releases.
target_sources(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/launcher/moa_update.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/launcher/moa_ui_sound.cpp")
# Game-side code: the START GAME choices (game/acg_quick_start.c, hooked with a
# function override on the title open; the launcher sets the choice through
# game/acg_start.h). Each disc has its own address table (game/acg_addrs_us.h,
# game/acg_addrs_jp.h); a disc whose table isn't complete yet (ACG_START_ALL 0)
# boots every start the normal way.
target_sources(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game/acg_quick_start.c")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/game/acg_quick_start.c"
    APPEND PROPERTY COMPILE_DEFINITIONS "ACG_REGION_JP=${ACG_REGION_JP}")
# Game-side code: Esc during play goes back to the launcher (game/acg_escape.c,
# registered as the title's in-game menu provider).
target_sources(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game/acg_escape.c")
target_include_directories(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game"
    "${CMAKE_CURRENT_SOURCE_DIR}/launcher"
    "${PSXRECOMP_ROOT}/runtime/include")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/codegen_setup.c"
    APPEND PROPERTY COMPILE_DEFINITIONS "ACG_REGION_JP=${ACG_REGION_JP}")
set_property(SOURCE "${ACG_RECOMP_UI_DIR}/src/common/backends/imgui/launcher_imgui.cpp"
    APPEND PROPERTY COMPILE_DEFINITIONS RECOMP_LAUNCHER_MOA_MENU=1
        "ACG_REGION_JP=${ACG_REGION_JP}")
if(PSX_RELEASE_CHANNEL STREQUAL "public")
    set(_acg_dev_updates 0)
else()
    set(_acg_dev_updates 1)
endif()
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/launcher/moa_update.cpp"
    APPEND PROPERTY COMPILE_DEFINITIONS
        "MOA_VERSION=\"${PSX_GAME_VERSION}\"" "MOA_DEV_UPDATES=${_acg_dev_updates}")

# Windows: "Armored Core Gen 1.exe", the small program the player starts. The
# release keeps the game in RecompData/ beside it, with Memory Card/ and Mods/
# next to them (launcher/acg_starter.c; scripts/package_prebuilt_release.sh
# lays the folder out). Built with the game so CI needs no extra target.
# Only the US build makes it; the JP program is reached through the US one.
if(WIN32 AND ACG_REGION STREQUAL "US")
    add_executable(acg-starter WIN32 "${CMAKE_CURRENT_SOURCE_DIR}/launcher/acg_starter.c")
    set_target_properties(acg-starter PROPERTIES
        OUTPUT_NAME "Armored Core Gen 1"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/starter")
    target_compile_definitions(acg-starter PRIVATE
        "ACG_GAME_EXE=L\"$<TARGET_FILE_NAME:psx-runtime>\"")
    if(MINGW)
        target_link_options(acg-starter PRIVATE -municode -static)
    endif()
    # The game's own icon, already compiled by the framework (APP_ICON).
    if(TARGET psx-runtime_app_icon)
        target_sources(acg-starter PRIVATE $<TARGET_OBJECTS:psx-runtime_app_icon>)
    endif()
    add_dependencies(psx-runtime acg-starter)
endif()
