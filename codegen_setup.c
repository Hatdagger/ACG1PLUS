/* Title config for psxrecomp/host/psxrecomp_codegen_host.
 * Wired only when the build opts into -DPSX_SETUP_WIZARD=ON. */

#include "codegen_setup.h"

#include "psxrecomp_codegen_host.h"

static const PsxrecompCodegenHostConfig kCodegenConfig = {
    .display_name = "Armored Core Gen 1",
    .project_root_env = "ARMORED_CORE_GEN_1_PROJECT_ROOT",
    .build_dir_env = "ARMORED_CORE_GEN_1_BUILD_DIR",
    .force_setup_env = "ARMORED_CORE_GEN_1_FORCE_SETUP",
    .psxrecomp_cli_relpath = "psxrecomp/psxrecomp_cli.py",
#if defined(ACG_REGION_JP) && ACG_REGION_JP
    /* The Japanese disc's program (CMakeLists.txt ACG_REGION=JP). */
    .seed_cfg_relpath = "game-jp.toml",
    .game_toml_relpath = "game-jp.toml",
    .gen_marker_relpath = "generated-jp/SLPS_018.55_dispatch.c",
    .build_dir_name = "build-release-jp",
#else
    .seed_cfg_relpath = "game.toml",
    .game_toml_relpath = "game.toml",
    .gen_marker_relpath = "generated/SLUS_010.30_dispatch.c",
    .build_dir_name = "build-release",
#endif
    .cmake_target = "psx-runtime",
    .exe_basename = "Armored_Core_Gen_1",
    .prepare_note =
        "Uses your legal disc with the local psxrecomp SDK to generate "
        "BIOS + game C, then cmake --build. The product lives under "
        "build-release/; reopening this setup exe forwards there.",
    .prepare_note_windows =
        "Uses your legal disc with the local psxrecomp SDK to generate "
        "BIOS + game C, then quits and rebuilds via a helper. Afterward, "
        "this setup exe forwards to build-release/ (bios, mods, settings).",
    .prepare_note_no_cmake =
        "Uses your legal disc with the local psxrecomp SDK to generate "
        "BIOS + game C. Rebuild into build-release/, then relaunch this "
        "setup exe (it forwards to the product build).",
};

void psx_game_codegen_setup_apply(RecompLauncherCGameInfo* gi) {
    psxrecomp_codegen_host_apply(gi, &kCodegenConfig);
}

void psx_game_codegen_relaunch_or_exit(const char* disc_path) {
    psxrecomp_codegen_host_relaunch_or_exit(disc_path);
}

void psx_game_codegen_forward_if_built(int argc, char** argv) {
    psxrecomp_codegen_host_forward_if_built(&kCodegenConfig, argc, argv);
}

