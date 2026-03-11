#ifndef CODEGEN_H
#define CODEGEN_H

#include "../ast/ast.h"
#include "analyzer.h"
#include <stdio.h>

// ─────────────────────────────────────────────
//  Value — an LLVM SSA register or constant
// ─────────────────────────────────────────────
typedef struct {
    char  name[64];   // e.g. "%3", "@add", "10", "3.14"
    char  lltype[64]; // e.g. "i32", "float", "i8*"
    int   is_ptr;     // 1 if this is an alloca (needs load before use)
} CGValue;

// ─────────────────────────────────────────────
//  Symbol in codegen scope (maps name → alloca)
// ─────────────────────────────────────────────
typedef struct CGSymbol CGSymbol;
struct CGSymbol {
    char      name[64];
    char      lltype[64];
    char      alloca_reg[64];  // the %name.addr alloca register
    CGSymbol *next;
};

typedef struct CGScope CGScope;
struct CGScope {
    CGSymbol *symbols;
    CGScope  *parent;
};

// ─────────────────────────────────────────────
//  Codegen state
// ─────────────────────────────────────────────
typedef struct {
    FILE     *out;           // output file (or stdout)
    int       reg_counter;   // SSA register counter (%0, %1, ...)
    int       label_counter; // label counter (if.then.1, loop.2, ...)
    CGScope  *scope;
    int       had_error;
    int       unsafe_depth;

    // string literal dedup table
    char    **str_literals;
    int      *str_ids;
    int       str_count;
} Codegen;

// ─────────────────────────────────────────────
//  API
// ─────────────────────────────────────────────
void codegen_init  (Codegen *cg, FILE *out);
void codegen_run   (Codegen *cg, ASTNode *program);
void codegen_free  (Codegen *cg);

#endif /* CODEGEN_H */