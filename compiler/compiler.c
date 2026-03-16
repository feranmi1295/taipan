#define _GNU_SOURCE
#include "compiler.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../ast/ast.h"
#include "analyzer.h"
#include "codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Load source file
// ─────────────────────────────────────────────
int compiler_load_source(CompilerOptions *opts) {
    FILE *f = fopen(opts->src_path, "rb");
    if (!f) {
        fprintf(stderr, "venom: cannot open '%s'\n", opts->src_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    opts->source = malloc(sz + 1);
    if (!opts->source) { fclose(f); return 1; }

    fread(opts->source, 1, sz, f);
    opts->source[sz] = '\0';
    fclose(f);
    return 0;
}

// ─────────────────────────────────────────────
//  Load all source files (multi-file)
// ─────────────────────────────────────────────
int compiler_load_all_sources(CompilerOptions *opts) {
    for (int i = 0; i < opts->n_files; i++) {
        FILE *f = fopen(opts->src_paths[i], "rb");
        if (!f) {
            fprintf(stderr, "venom: cannot open '%s'\n", opts->src_paths[i]);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        rewind(f);
        opts->sources[i] = malloc((size_t)sz + 1);
        fread(opts->sources[i], 1, (size_t)sz, f);
        opts->sources[i][sz] = '\0';
        fclose(f);
    }
    return 0;
}

// ─────────────────────────────────────────────
//  Merge multiple AST programs into one
//  All top-level nodes from extra files are
//  appended to the first program's stmt list
// ─────────────────────────────────────────────
static ASTNode *merge_programs(ASTNode **programs, int n) {
    if (n == 0) return NULL;
    if (n == 1) return programs[0];
    ASTNode *base = programs[0];
    for (int i = 1; i < n; i++) {
        ASTNode *extra = programs[i];
        if (!extra) continue;
        int base_count  = base->as.program.count;
        int extra_count = extra->as.program.count;
        int new_count   = base_count + extra_count;
        base->as.program.stmts = realloc(base->as.program.stmts,
            sizeof(ASTNode*) * (size_t)new_count);
        for (int j = 0; j < extra_count; j++)
            base->as.program.stmts[base_count + j] = extra->as.program.stmts[j];
        base->as.program.count = new_count;
        // free only the wrapper, not the nodes (they're merged)
        free(extra->as.program.stmts);
        free(extra);
    }
    return base;
}

// ─────────────────────────────────────────────
//  Derive output path from input path
//  e.g.  hello.tp  →  hello.ll
// ─────────────────────────────────────────────
static void derive_out_path(const char *src, char *buf, int bufsz) {
    strncpy(buf, src, bufsz - 4);
    buf[bufsz - 4] = '\0';
    // strip .tp extension if present
    int len = (int)strlen(buf);
    if (len > 3 && !strcmp(buf + len - 3, ".tp"))
        buf[len - 3] = '\0';
    strncat(buf, ".ll", bufsz - 1);
}

// ─────────────────────────────────────────────
//  compiler_run — full pipeline
// ─────────────────────────────────────────────
int compiler_run(CompilerOptions *opts) {
    int exit_code = 0;

    // ── Multi-file: parse all source files ───
    int n = opts->n_files > 0 ? opts->n_files : 1;
    ASTNode **programs = calloc((size_t)n, sizeof(ASTNode*));

    for (int fi = 0; fi < n; fi++) {
        const char *src_text = (opts->n_files > 0)
            ? opts->sources[fi]
            : opts->source;
        const char *src_name = (opts->n_files > 0)
            ? opts->src_paths[fi]
            : opts->src_path;

        if (opts->verbose)
            fprintf(stderr, "[venom] Lexing %s ...\n", src_name);

        Lexer lexer;
        lexer_init(&lexer, src_text);

        if (fi == 0 && opts->emit_tokens) {
            fprintf(stdout, "; ── Token stream ────────────────────────\n");
            Lexer tmp; lexer_init(&tmp, src_text);
            while (1) {
                Token tok = lexer_next_token(&tmp);
                if (tok.type == TOK_NEWLINE || tok.type == TOK_COMMENT) continue;
                token_print(&tok);
                if (tok.type == TOK_EOF || tok.type == TOK_ERROR) break;
            }
            fprintf(stdout, "\n");
            lexer_init(&lexer, src_text);
        }

        if (opts->verbose)
            fprintf(stderr, "[venom] Parsing %s ...\n", src_name);

        Parser parser;
        parser_init(&parser, &lexer);
        programs[fi] = parser_run(&parser);

        opts->had_parse_error |= parser.had_error;
        if (parser.had_error) {
            fprintf(stderr, "venom: parse errors in '%s'\n", src_name);
            for (int k=0;k<=fi;k++) if(programs[k]) ast_free(programs[k]);
            free(programs);
            return 1;
        }
    }

    // ── Merge all ASTs into one program ───────
    ASTNode *program = merge_programs(programs, n);
    free(programs);

    if (opts->emit_ast) {
        fprintf(stdout, "; ── AST ─────────────────────────────────\n");
        ast_print(program, 0);
        fprintf(stdout, "\n");
    }

    // ── Stage 3: Semantic analysis ───────────
    if (opts->verbose) fprintf(stderr, "[venom] Analyzing ...\n");

    Analyzer analyzer;
    analyzer_init(&analyzer);
    analyzer_run(&analyzer, program);

    opts->had_sem_error = analyzer.had_error;
    if (analyzer.had_error) {
        fprintf(stderr, "venom: semantic errors\n");
        analyzer_free(&analyzer);
        ast_free(program);
        return 1;
    }

    // ── Stage 4: Codegen → LLVM IR ───────────
    if (opts->verbose) fprintf(stderr, "[venom] Generating IR ...\n");

    // Determine output path
    char out_buf[512];
    const char *out_path = opts->out_path;
    if (!out_path) {
        // use primary src path (first file)
        const char *primary = opts->src_path ? opts->src_path :
                              (opts->n_files > 0 ? opts->src_paths[0] : NULL);
        if (primary)
            derive_out_path(primary, out_buf, sizeof(out_buf));
        else
            strncpy(out_buf, "output.ll", sizeof(out_buf));
        out_path = out_buf;
    }

    FILE *ir_file = fopen(out_path, "w");
    if (!ir_file) {
        fprintf(stderr, "venom: cannot write to '%s'\n", out_path);
        analyzer_free(&analyzer);
        ast_free(program);
        return 1;
    }

    Codegen codegen;
    codegen_init(&codegen, ir_file);
    codegen_run(&codegen, program);
    fclose(ir_file);

    opts->had_cg_error = codegen.had_error;
    if (codegen.had_error) {
        fprintf(stderr, "venom: codegen errors\n");
        exit_code = 1;
    } else {
        if (opts->verbose || opts->emit_ir)
            fprintf(stderr, "[venom] IR written to %s\n", out_path);
    }

    codegen_free(&codegen);

    // ── Stage 5: Optional — run llc ──────────
    if (!exit_code && opts->run_llc) {
        // derive .s path from .ll path
        char s_path[512];
        strncpy(s_path, out_path, sizeof(s_path) - 1);
        int slen = (int)strlen(s_path);
        if (slen > 3 && !strcmp(s_path + slen - 3, ".ll"))
            s_path[slen - 3] = '\0';
        strncat(s_path, ".s", sizeof(s_path) - 1);

        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "llc -filetype=asm -o %s %s", s_path, out_path);
        if (opts->verbose) fprintf(stderr, "[venom] Running: %s\n", cmd);
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "venom: llc failed (is LLVM installed?)\n");
            exit_code = 1;
        } else if (opts->verbose) {
            fprintf(stderr, "[venom] Assembly written to %s\n", s_path);
        }
    }

    // ── Stage 6: Optional — link to binary ───
    if (!exit_code && opts->run_link) {
        char bin_path[512];
        strncpy(bin_path, opts->src_path, sizeof(bin_path) - 1);
        int blen = (int)strlen(bin_path);
        if (blen > 3 && !strcmp(bin_path + blen - 3, ".tp"))
            bin_path[blen - 3] = '\0';

        // find best available linker
        // try common install locations
        const char *candidates[] = {
            "clang", "clang-20", "clang-18", "clang-17",
            "/usr/bin/clang", "/usr/bin/clang-20", "/usr/bin/clang-18",
            "/usr/local/bin/clang", NULL
        };
        const char *clang_bin = NULL;
        for (int ci = 0; candidates[ci]; ci++) {
            char probe[128];
            snprintf(probe, sizeof(probe), "%s --version > /dev/null 2>&1", candidates[ci]);
            if (system(probe) == 0) { clang_bin = candidates[ci]; break; }
        }
        const char *llc_candidates[] = {
            "llc", "llc-20", "llc-18", "llc-17",
            "/usr/bin/llc", "/usr/bin/llc-20", "/usr/bin/llc-18", NULL
        };
        const char *llc_bin = NULL;
        for (int ci = 0; llc_candidates[ci]; ci++) {
            char probe[128];
            snprintf(probe, sizeof(probe), "%s --version > /dev/null 2>&1", llc_candidates[ci]);
            if (system(probe) == 0) { llc_bin = llc_candidates[ci]; break; }
        }

        char cmd[1024];
        int rc = -1;

        if (clang_bin) {
            // clang can compile .ll directly
            snprintf(cmd, sizeof(cmd), "%s -o %s %s /home/abiodunidris/taipan/taipan/runtime/libtaipan.a -lm", clang_bin, bin_path, out_path);
            if (opts->verbose) fprintf(stderr, "[venom] Running: %s\n", cmd);
            rc = system(cmd);
        } else if (llc_bin) {
            // llc .ll -> .o, then gcc to link
            char obj_path[520];
            snprintf(obj_path, sizeof(obj_path), "%s.o", bin_path);
            snprintf(cmd, sizeof(cmd), "%s -filetype=obj -o %s %s", llc_bin, obj_path, out_path);
            if (opts->verbose) fprintf(stderr, "[venom] Running: %s\n", cmd);
            rc = system(cmd);
            if (rc == 0) {
                snprintf(cmd, sizeof(cmd), "gcc -o %s %s /home/abiodunidris/taipan/taipan/runtime/libtaipan.a -lm", bin_path, obj_path);
                if (opts->verbose) fprintf(stderr, "[venom] Running: %s\n", cmd);
                rc = system(cmd);
                remove(obj_path);
            }
        }

        if (rc != 0) {
            fprintf(stderr, "venom: link failed. Install LLVM tools:\n");
            fprintf(stderr, "       sudo apt install llvm clang\n");
            exit_code = 1;
        } else if (opts->verbose) {
            fprintf(stderr, "[venom] Executable: %s\n", bin_path);
        }
    }

    // ── Cleanup ───────────────────────────────
    analyzer_free(&analyzer);
    ast_free(program);

    if (!exit_code && opts->verbose)
        fprintf(stderr, "[venom] Done.\n");

    return exit_code;
}

void compiler_free(CompilerOptions *opts) {
    free(opts->source);
    opts->source = NULL;
    for (int i = 0; i < opts->n_files; i++) {
        free(opts->sources[i]);
        opts->sources[i] = NULL;
    }
}