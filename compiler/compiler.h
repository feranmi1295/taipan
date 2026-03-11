#ifndef COMPILER_H
#define COMPILER_H

// ─────────────────────────────────────────────
//  Venom — Taipan Compiler Driver
//  Orchestrates: source → lex → parse → analyze → codegen → LLVM IR
// ─────────────────────────────────────────────

typedef struct {
    // Input
    const char *src_path;      // path to .tp file
    char       *source;        // loaded source text

    // Output control
    const char *out_path;      // output .ll path (NULL = derive from src)
    int         emit_tokens;   // --emit-tokens
    int         emit_ast;      // --emit-ast
    int         emit_ir;       // --emit-ir (default)
    int         run_llc;       // --compile: run llc after IR
    int         run_link;      // --link: run clang to produce executable
    int         verbose;       // --verbose

    // Results
    int         had_lex_error;
    int         had_parse_error;
    int         had_sem_error;
    int         had_cg_error;
} CompilerOptions;

// Load a .tp source file into opts->source (caller must free)
int  compiler_load_source (CompilerOptions *opts);

// Run full pipeline. Returns 0 on success.
int  compiler_run         (CompilerOptions *opts);

// Free loaded source
void compiler_free        (CompilerOptions *opts);

// Print usage
void compiler_usage       (const char *argv0);

#endif /* COMPILER_H */