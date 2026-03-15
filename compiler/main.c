#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Usage
// ─────────────────────────────────────────────
void compiler_usage(const char *argv0) {
    fprintf(stderr,
        "Venom — The Taipan Compiler\n"
        "Usage: %s [options] <file.tp>\n"
        "\n"
        "Options:\n"
        "  --emit-tokens     Print token stream and continue\n"
        "  --emit-ast        Print AST and continue\n"
        "  --emit-ir         Print IR output path after writing\n"
        "  --compile         Run llc to produce assembly (.s)\n"
        "  --link            Run clang to produce executable binary\n"
        "  --output <path>   Set output .ll path (default: <file>.ll)\n"
        "  --verbose         Print each compilation stage\n"
        "  --help            Show this message\n"
        "\n"
        "Examples:\n"
        "  %s hello.tp                     # emit hello.ll\n"
        "  %s hello.tp --link              # emit hello.ll and link to ./hello\n"
        "  %s hello.tp --emit-ast --verbose\n"
        "  %s hello.tp --output out.ll\n"
        "\n"
        "Pipeline:\n"
        "  .tp source\n"
        "    → Lexer      (tokenize)\n"
        "    → Parser     (AST)\n"
        "    → Analyzer   (type check, unsafe check)\n"
        "    → Codegen    (LLVM IR .ll)\n"
        "    → llc        (assembly .s)    [--compile]\n"
        "    → clang      (binary)         [--link]\n",
        argv0, argv0, argv0, argv0, argv0
    );
}

// ─────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────
int main(int argc, char **argv) {
    if (argc < 2) {
        compiler_usage(argv[0]);
        return 1;
    }

    CompilerOptions opts;
    memset(&opts, 0, sizeof(opts));

    // ── Parse arguments ───────────────────────
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            compiler_usage(argv[0]);
            return 0;
        }
        else if (!strcmp(argv[i], "--emit-tokens")) opts.emit_tokens = 1;
        else if (!strcmp(argv[i], "--emit-ast"))    opts.emit_ast    = 1;
        else if (!strcmp(argv[i], "--emit-ir"))     opts.emit_ir     = 1;
        else if (!strcmp(argv[i], "--compile"))     opts.run_llc     = 1;
        else if (!strcmp(argv[i], "--link"))        opts.run_link    = 1;
        else if (!strcmp(argv[i], "--verbose"))     opts.verbose     = 1;
        else if (!strcmp(argv[i], "--output") || !strcmp(argv[i], "-o")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "venom: --output requires a path\n");
                return 1;
            }
            opts.out_path = argv[++i];
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "venom: unknown option '%s'\n", argv[i]);
            compiler_usage(argv[0]);
            return 1;
        }
        else {
            // Accept multiple .tp files
            if (opts.n_files >= MAX_SOURCE_FILES) {
                fprintf(stderr, "venom: too many input files (max %d)\n", MAX_SOURCE_FILES);
                return 1;
            }
            opts.src_paths[opts.n_files] = argv[i];
            opts.n_files++;
            if (!opts.src_path) opts.src_path = argv[i]; // primary = first
        }
    }

    if (!opts.src_path) {
        fprintf(stderr, "venom: no input file\n");
        compiler_usage(argv[0]);
        return 1;
    }

    // ── Check .tp extension ───────────────────
    int namelen = (int)strlen(opts.src_path);
    if (namelen < 4 || strcmp(opts.src_path + namelen - 3, ".tp") != 0) {
        fprintf(stderr, "venom: warning: input file '%s' does not have .tp extension\n",
                opts.src_path);
    }

    // ── Resolve use statements to local files ─
    // Scan primary file for "use <name>" that match local .tp files
    // e.g. "use math" -> looks for math.tp in same dir
    if (opts.n_files == 1) {
        FILE *scan = fopen(opts.src_paths[0], "r");
        if (scan) {
            char line[512];
            // Get directory of primary file
            char dir[512] = ".";
            const char *slash = strrchr(opts.src_paths[0], '/');
            if (slash) {
                size_t dlen = (size_t)(slash - opts.src_paths[0]);
                strncpy(dir, opts.src_paths[0], dlen);
                dir[dlen] = '\0';
            }
            while (fgets(line, sizeof(line), scan) && opts.n_files < MAX_SOURCE_FILES) {
                // Skip std library uses
                if (strncmp(line, "use std.", 8) == 0) continue;
                if (strncmp(line, "use ", 4) != 0) continue;
                // Extract module name
                char modname[256] = {0};
                int mi = 0;
                const char *p = line + 4;
                while (*p && *p != '\n' && *p != ' ' && *p != '\r' && mi < 255)
                    modname[mi++] = *p++;
                modname[mi] = '\0';
                if (mi == 0) continue;
                // Build file path: dir/modname.tp
                static char fpath[MAX_SOURCE_FILES][512];
                snprintf(fpath[opts.n_files], 512, "%s/%s.tp", dir, modname);
                // Check if file exists
                FILE *test = fopen(fpath[opts.n_files], "r");
                if (test) {
                    fclose(test);
                    // Add to file list if not already there
                    int already = 0;
                    for (int k = 0; k < opts.n_files; k++)
                        if (!strcmp(opts.src_paths[k], fpath[opts.n_files])) { already=1; break; }
                    if (!already) {
                        opts.src_paths[opts.n_files] = fpath[opts.n_files];
                        opts.n_files++;
                        if (opts.verbose)
                            fprintf(stderr, "[venom] auto-importing %s\n", fpath[opts.n_files-1]);
                    }
                }
            }
            fclose(scan);
        }
    }

    // ── Load source(s) ────────────────────────
    if (opts.n_files > 0) {
        if (compiler_load_all_sources(&opts) != 0)
            return 1;
    } else {
        // legacy single file
        opts.src_paths[0] = opts.src_path;
        opts.n_files = 1;
        if (compiler_load_all_sources(&opts) != 0)
            return 1;
    }

    // ── Run pipeline ──────────────────────────
    int result = compiler_run(&opts);

    // ── Summary ───────────────────────────────
    if (result == 0 && !opts.verbose) {
        // derive output path for display
        char out_display[512];
        if (opts.out_path) {
            strncpy(out_display, opts.out_path, sizeof(out_display) - 1);
        } else {
            int len = namelen;
            strncpy(out_display, opts.src_path, sizeof(out_display) - 4);
            out_display[sizeof(out_display) - 4] = '\0';
            int dl = (int)strlen(out_display);
            if (dl > 3 && !strcmp(out_display + dl - 3, ".tp"))
                out_display[dl - 3] = '\0';
            strncat(out_display, ".ll", sizeof(out_display) - 1);
            (void)len;
        }
        if (opts.n_files > 1)
            fprintf(stderr, "venom: compiled %d files → %s\n", opts.n_files, out_display);
        else
            fprintf(stderr, "venom: compiled '%s' → %s\n", opts.src_path, out_display);
    }

    compiler_free(&opts);
    return result;
}