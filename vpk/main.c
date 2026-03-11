#define _GNU_SOURCE
#include "vpk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        vpk_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    // ── init ─────────────────────────────────
    if (!strcmp(cmd, "init")) {
        return vpk_init(argc > 2 ? argv[2] : ".");
    }

    // ── install ───────────────────────────────
    if (!strcmp(cmd, "install")) {
        if (argc < 3) {
            fprintf(stderr, "vpk: install requires a package name\n");
            fprintf(stderr, "     usage: vpk install <package>[@version]\n");
            return 1;
        }
        // parse pkg@version syntax
        char pkg_name[VPK_MAX_NAME] = {0};
        char version[VPK_MAX_VERSION] = {0};
        char *at = strchr(argv[2], '@');
        if (at) {
            strncpy(pkg_name, argv[2], (size_t)(at - argv[2]));
            strncpy(version,  at + 1,  VPK_MAX_VERSION-1);
        } else {
            strncpy(pkg_name, argv[2], VPK_MAX_NAME-1);
            strncpy(version,  "*",     VPK_MAX_VERSION-1);
        }
        return vpk_install(pkg_name, version);
    }

    // ── remove ────────────────────────────────
    if (!strcmp(cmd, "remove") || !strcmp(cmd, "uninstall") || !strcmp(cmd, "rm")) {
        if (argc < 3) {
            fprintf(stderr, "vpk: remove requires a package name\n");
            return 1;
        }
        return vpk_remove(argv[2]);
    }

    // ── list ──────────────────────────────────
    if (!strcmp(cmd, "list") || !strcmp(cmd, "ls")) {
        return vpk_list();
    }

    // ── search ────────────────────────────────
    if (!strcmp(cmd, "search")) {
        return vpk_search(argc > 2 ? argv[2] : NULL);
    }

    // ── info ──────────────────────────────────
    if (!strcmp(cmd, "info")) {
        return vpk_info(argc > 2 ? argv[2] : NULL);
    }

    // ── update ────────────────────────────────
    if (!strcmp(cmd, "update") || !strcmp(cmd, "upgrade")) {
        return vpk_update();
    }

    // ── publish ───────────────────────────────
    if (!strcmp(cmd, "publish")) {
        return vpk_publish();
    }

    // ── version ───────────────────────────────
    if (!strcmp(cmd, "--version") || !strcmp(cmd, "version")) {
        printf("vpk %s\n", VPK_VERSION);
        return 0;
    }

    // ── help ──────────────────────────────────
    if (!strcmp(cmd, "--help") || !strcmp(cmd, "help")) {
        vpk_usage(argv[0]);
        return 0;
    }

    fprintf(stderr, "vpk: unknown command '%s'\n\n", cmd);
    vpk_usage(argv[0]);
    return 1;
}