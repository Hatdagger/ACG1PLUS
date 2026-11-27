# ACG1PLUS launcher (launcher2): one program, no game code, linked into a single file.
#
# Needs SDL3 as a static library (SDL3_DIR -> its CMake files). The compiler
# packs carry one: cmake-clang-v1 v1.0.14 deps/lib/cmake/SDL3 (SDL 3.4.10), the
# same SDL the game is built with. Dear ImGui comes from the recomp-ui submodule.
# How to build both files: launcher2/build.sh, README.md.
cmake_minimum_required(VERSION 3.20)
project(acg1plus_launcher LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ACG_LAUNCHER_VERSION)
string(STRIP "${ACG_LAUNCHER_VERSION}" ACG_LAUNCHER_VERSION)

set(RECOMP_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../recomp-ui" CACHE PATH "recomp-ui checkout")
set(IMGUI_DIR "${RECOMP_UI_DIR}/src/third_party/imgui")
set(FONT_DIR "${RECOMP_UI_DIR}/assets/common/fonts")
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR "Dear ImGui not found in ${IMGUI_DIR}: run `git submodule update --init recomp-ui`")
endif()

find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3-static)

# The disc this launcher looks for: game.toml [prepare_disc] known_sizes / known_md5
# (the same values the build's verify-disc checks), read here so they are never
# typed in twice.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../game.toml" ACG_GAME_TOML)
string(REGEX MATCH "known_sizes *= *\\[([^]]*)\\]" _m "${ACG_GAME_TOML}")
string(REGEX REPLACE "[^0-9,]" "" ACG_DISC_SIZES "${CMAKE_MATCH_1}")
string(REGEX MATCH "known_md5 *= *\\[([^]]*)\\]" _m "${ACG_GAME_TOML}")
string(REGEX REPLACE "[^0-9a-fA-F,]" "" ACG_DISC_MD5 "${CMAKE_MATCH_1}")
if(ACG_DISC_SIZES STREQUAL "" OR ACG_DISC_MD5 STREQUAL "")
    message(FATAL_ERROR "Could not read known_sizes / known_md5 from game.toml")
endif()
message(STATUS "Disc: sizes ${ACG_DISC_SIZES}, md5 ${ACG_DISC_MD5}")

# The source zip that goes with this launcher (acg1plus-source-<version>.zip on the
# same release), baked in by the release workflow (launcher2/release.sh). Empty in a
# local build: then the launcher needs --source-zip.
set(ACG_SOURCE_URL "" CACHE STRING "URL of the source zip on the release")
set(ACG_SOURCE_SHA256 "" CACHE STRING "SHA-256 of the source zip")
set(ACG_SOURCE_SIZE "0" CACHE STRING "Size of the source zip in bytes")
set(ACG_SOURCE_VERSION "" CACHE STRING "Version in the source zip's name")

# The menu fonts (LatoLatin Regular + Bold, SIL Open Font License 1.1) built into the program.
set(FONTS_C "${CMAKE_CURRENT_BINARY_DIR}/fonts.c")
add_custom_command(
    OUTPUT "${FONTS_C}"
    COMMAND "${CMAKE_COMMAND}" -DOUT=${FONTS_C}
            -DREGULAR=${FONT_DIR}/LatoLatin-Regular.ttf -DBOLD=${FONT_DIR}/LatoLatin-Bold.ttf
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake"
    DEPENDS "${FONT_DIR}/LatoLatin-Regular.ttf" "${FONT_DIR}/LatoLatin-Bold.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake"
    COMMENT "Building the menu fonts into the program")

add_executable(acg1plus_launcher WIN32
    src/main.cpp
    src/menu.cpp
    src/ui.cpp
    src/firstrun.cpp
    src/disc.cpp
    src/setup.cpp
    src/play.cpp
    src/platform.cpp
    src/hash.cpp
    src/unzip.cpp
    third_party/miniz/miniz.c
    "${FONTS_C}"
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")
set_target_properties(acg1plus_launcher PROPERTIES OUTPUT_NAME "ACG1PLUS")
target_include_directories(acg1plus_launcher PRIVATE "${IMGUI_DIR}" "${IMGUI_DIR}/backends" src
    third_party/miniz)
target_compile_definitions(acg1plus_launcher PRIVATE
    ACG_LAUNCHER_VERSION="${ACG_LAUNCHER_VERSION}"
    ACG_DISC_SIZES="${ACG_DISC_SIZES}"
    ACG_DISC_MD5="${ACG_DISC_MD5}"
    ACG_SOURCE_URL="${ACG_SOURCE_URL}"
    ACG_SOURCE_SHA256="${ACG_SOURCE_SHA256}"
    ACG_SOURCE_SIZE=${ACG_SOURCE_SIZE}ull
    ACG_SOURCE_VERSION="${ACG_SOURCE_VERSION}"
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
# miniz (unzip, MIT licence, third_party/miniz): plain C, quiet about its own style.
set_source_files_properties(third_party/miniz/miniz.c PROPERTIES COMPILE_OPTIONS "-w")
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(acg1plus_launcher PRIVATE Threads::Threads)
endif()
target_link_libraries(acg1plus_launcher PRIVATE SDL3::SDL3-static)

if(WIN32)
    # Version information and the manifest (UTF-8 file names, sharp on high-DPI screens).
    string(REPLACE "." "," ACG_VER_COMMAS "${ACG_LAUNCHER_VERSION},0")
    set(ACG_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
    configure_file(res/launcher.rc.in "${CMAKE_CURRENT_BINARY_DIR}/launcher.rc" @ONLY)
    target_sources(acg1plus_launcher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/launcher.rc")
    # Nothing beside the exe: the C++ runtime and winpthreads go inside it.
    target_link_options(acg1plus_launcher PRIVATE -static)
else()
    # The C++ runtime goes inside; only the C library and libgcc_s (both on every
    # SteamOS) stay outside. SDL opens X11, OpenGL and sound by itself when it runs.
    target_link_options(acg1plus_launcher PRIVATE -static-libstdc++)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(acg1plus_launcher PRIVATE -s)
endif()
