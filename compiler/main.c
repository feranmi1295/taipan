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
            if (opts.src_path) {
                fprintf(stderr, "venom: multiple input files not supported\n");
                return 1;
            }
            opts.src_path = argv[i];
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

    // ── Load source ───────────────────────────
    if (compiler_load_source(&opts) != 0)
        return 1;

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
        fprintf(stderr, "venom: compiled '%s' → %s\n", opts.src_path, out_display);
    }

    compiler_free(&opts);
    return result;
}