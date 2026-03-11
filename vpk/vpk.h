#ifndef VPK_H
#define VPK_H

// ─────────────────────────────────────────────
//  VPK — Taipan Package Manager
//
//  Commands:
//    vpk init                  create taipan.pkg in current dir
//    vpk install <pkg>         install a package
//    vpk remove  <pkg>         remove a package
//    vpk list                  list installed packages
//    vpk search  <query>       search registry
//    vpk publish               publish current package
//    vpk update                update all packages
//    vpk info    <pkg>         show package info
//
//  Files:
//    taipan.pkg                project manifest (JSON-like)
//    .vpk/                     local package cache
//    .vpk/packages/            installed packages
//    .vpk/lock.pkg             lockfile (exact versions)
// ─────────────────────────────────────────────

#define VPK_VERSION      "0.1.0"
#define VPK_MANIFEST     "taipan.pkg"
#define VPK_LOCKFILE     ".vpk/lock.pkg"
#define VPK_PKG_DIR      ".vpk/packages"
#define VPK_REGISTRY_URL "https://registry.taipan-lang.org"
#define VPK_MAX_DEPS     64
#define VPK_MAX_NAME     128
#define VPK_MAX_VERSION  32

// ── Dependency entry ─────────────────────────
typedef struct {
    char name[VPK_MAX_NAME];
    char version[VPK_MAX_VERSION];   // e.g. "1.2.3" or "*"
    int  dev_only;                   // dev dependency flag
} VpkDep;

// ── Package manifest ─────────────────────────
typedef struct {
    char    name[VPK_MAX_NAME];
    char    version[VPK_MAX_VERSION];
    char    description[256];
    char    author[128];
    char    license[32];
    char    entry[256];              // main source file
    VpkDep  deps[VPK_MAX_DEPS];
    int     dep_count;
    VpkDep  dev_deps[VPK_MAX_DEPS];
    int     dev_dep_count;
} VpkManifest;

// ── Installed package record ─────────────────
typedef struct {
    char name[VPK_MAX_NAME];
    char version[VPK_MAX_VERSION];
    char install_path[512];
    char checksum[65];               // SHA256 hex
} VpkInstalled;

// ── API ──────────────────────────────────────
int  vpk_init     (const char *dir);
int  vpk_install  (const char *pkg_name, const char *version);
int  vpk_remove   (const char *pkg_name);
int  vpk_list     (void);
int  vpk_search   (const char *query);
int  vpk_publish  (void);
int  vpk_update   (void);
int  vpk_info     (const char *pkg_name);

// Manifest I/O
int  vpk_manifest_read  (const char *path, VpkManifest *m);
int  vpk_manifest_write (const char *path, const VpkManifest *m);
void vpk_manifest_print (const VpkManifest *m);

// Lockfile I/O
int  vpk_lock_read  (const char *path, VpkInstalled *pkgs, int *count);
int  vpk_lock_write (const char *path, const VpkInstalled *pkgs, int count);

// Utilities
void vpk_usage   (const char *argv0);
int  vpk_ensure_dirs (void);

#endif /* VPK_H */