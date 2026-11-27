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

project(ACG1PLUS LANGUAGES C CXX)

# --- Opt-in product surfaces (OFF by default in the framework) -------------
# recomp-ui submodule present (PSX_RECOMP_UI defaults ON).
if(EXISTS "${PSXRECOMP_ROOT}/lib/recomp-net/CMakeLists.txt")
    set(PSX_NETPLAY ON CACHE BOOL
        "Link recomp-net delay-sync (opt-in; needs recomp-net)" FORCE)
endif()
# The framework's first-run wizard (Generate & rebuild inside the game) is OFF:
# its toolchain step installs the compiler under AppData and adds it to the
# user's PATH, and this game must stay in one portable folder. The game is
# built by scripts/windows/build.ps1 instead (docs/building.md); our own
# "preparing game" screen comes at ladder step 0.2.
set(PSX_SETUP_WIZARD OFF CACHE BOOL
    "Advertise first-run setup wizard + Generate & rebuild in recomp-ui" FORCE)

# --- Small patches to the two framework submodules ----------------------------
# patches/recomp-ui/: a hook for this title's own launcher menu.
# patches/psxrecomp/: PSX_MEDIA_CACHE_DIR, so the media import cache can stay in
# the game folder (portable install). Each patch is applied to the submodule's
# working tree here and left alone when already applied; a framework bump that
# breaks one stops the configure so it gets refreshed.
# The source zip on a release (tools/release/make_source_zip.sh) comes with the
# patches already applied and says so in patches/APPLIED.txt; players have no
# git, so nothing is applied then.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/patches/APPLIED.txt")
    set(_acg_patch_subs "")
    message(STATUS "Framework patches already applied (patches/APPLIED.txt)")
else()
    set(_acg_patch_subs recomp-ui psxrecomp)
    find_package(Git QUIET)
endif()
foreach(_acg_sub IN LISTS _acg_patch_subs)
    file(GLOB _acg_patches "${CMAKE_CURRENT_SOURCE_DIR}/patches/${_acg_sub}/*.patch")
    list(SORT _acg_patches)
    if(_acg_patches AND NOT GIT_FOUND)
        message(FATAL_ERROR "git is needed to apply the patches in patches/${_acg_sub}")
    endif()
    foreach(_acg_patch IN LISTS _acg_patches)
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${_acg_patch}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_acg_sub}" RESULT_VARIABLE _acg_applied
            OUTPUT_QUIET ERROR_QUIET)
        if(NOT _acg_applied EQUAL 0)
            execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${_acg_patch}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_acg_sub}" RESULT_VARIABLE _acg_apply
                ERROR_VARIABLE _acg_apply_err)
            if(NOT _acg_apply EQUAL 0)
                message(FATAL_ERROR "${_acg_sub} patch does not apply: ${_acg_patch}\n${_acg_apply_err}")
            endif()
            message(STATUS "Applied ${_acg_sub} patch ${_acg_patch}")
        endif()
    endforeach()
endforeach()
set(ACG_RECOMP_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui")

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

# --- The game ---------------------------------------------------------------
# Armored Core: Project Phantasma, USA (SLUS-00670), the only disc this project
# builds since 2026-11-26 (the Master of Arena configs are in reference/moa-build/).
# generated/ holds the game C that psxrecomp_cli.py generate writes from the
# player's own disc, on the player's own PC: the main program
# (SLUS_006.70_*) and, from aot/overlays.json, the code the game loads from the
# disc while playing (overlays_static*.c). Never committed (.gitignore).
psxrecomp_add_game_runtime(psx-runtime
    PRELOADED_MODS_DIR "${PSX_PRELOADED_MODS}"
    ENABLE_NETPLAY_IF_PRESENT
    NETPLAY_LOBBY_URL "ws://netplay.retcomm.net:8765"
    WINDOW_TITLE "Armored Core: Project Phantasma"
    EXE_NAME "ACG1PLUS_Game"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 2
    APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/psxrecomp.ico"
    # Before the list arguments below (GEN_FULL_GLOB, CODEGEN_SETUP_SOURCES),
    # which would swallow it.
    GAME_OVERLAY_STATIC_C "${CMAKE_CURRENT_SOURCE_DIR}/generated/overlays_static.c"
    GEN_MARKER "generated/SLUS_006.70_dispatch.c"
    GEN_FULL_GLOB "generated/SLUS_006.70_full_*.c"
    CODEGEN_SETUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/codegen_setup.c"
)

# Stage runtime configs next to the exe for direct build/ launches. Only copy
# files that exist — never fail POST_BUILD when game_options.toml is absent.
foreach(_PSX_RUNTIME_CONFIG game.toml game_options.toml)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}"
                "$<TARGET_FILE_DIR:psx-runtime>/${_PSX_RUNTIME_CONFIG}"
            COMMENT "Staging ${_PSX_RUNTIME_CONFIG}"
            VERBATIM)
    endif()
endforeach()

# The launcher's own menu (launcher/moa_menu.inc, drawn inside recomp-ui's
# launcher) and its updater. The dev channel (Actions builds with a GitHub
# token) is compiled out of public releases.
target_sources(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/launcher/moa_update.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/launcher/moa_ui_sound.cpp")
# Game-side code: the START GAME choices. Phantasma has no quick-start hooks
# yet (ladder 0.3), so game/acg_start_none.c makes every start a normal boot.
# The Master of Arena version is kept for reference in reference/moa-build/.
# game/acg_portable.c keeps every cache and launcher file inside the game
# folder (portable install; runs before the game's main()).
target_sources(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game/acg_start_none.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/game/acg_portable.c")
# Game-side code: Esc during play goes back to the launcher (game/acg_escape.c,
# registered as the title's in-game menu provider).
target_sources(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game/acg_escape.c")
# Game-side code: missions from other games on Phantasma's missions engine (AC1 import pilot,
# docs/ac1-import.md): files from the content packs, the engine's fixed message lines, the per-mission
# value, and the mission chain table. Project Phantasma USA addresses; each hook checks its code words.
if(NOT ACG_REGION STREQUAL "JP")
    target_sources(psx-runtime PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/game/acg_mission_files.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/acg_mission_chain.c")
endif()
target_include_directories(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game"
    "${CMAKE_CURRENT_SOURCE_DIR}/launcher"
    "${PSXRECOMP_ROOT}/runtime/include")
set_property(SOURCE "${ACG_RECOMP_UI_DIR}/src/common/backends/imgui/launcher_imgui.cpp"
    APPEND PROPERTY COMPILE_DEFINITIONS RECOMP_LAUNCHER_MOA_MENU=1)
if(PSX_RELEASE_CHANNEL STREQUAL "public")
    set(_acg_dev_updates 0)
else()
    set(_acg_dev_updates 1)
endif()
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/launcher/moa_update.cpp"
    APPEND PROPERTY COMPILE_DEFINITIONS
        "MOA_VERSION=\"${PSX_GAME_VERSION}\"" "MOA_DEV_UPDATES=${_acg_dev_updates}")

# Windows: "ACG1PLUS.exe", the small program the player starts. It sits at the
# top of the portable folder and starts RecompData/windows/ACG1PLUS_Game.exe
# (launcher/acg_starter.c; scripts/windows/build.ps1 lays the folder out,
# docs/building.md). Built with the game so no extra target is needed.
if(WIN32)
    add_executable(acg-starter WIN32 "${CMAKE_CURRENT_SOURCE_DIR}/launcher/acg_starter.c")
    set_target_properties(acg-starter PROPERTIES
        OUTPUT_NAME "ACG1PLUS"
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
