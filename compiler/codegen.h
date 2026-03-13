#ifndef CODEGEN_H
#define CODEGEN_H

#include "../ast/ast.h"
#include "analyzer.h"
#include <stdio.h>

typedef struct {
    char  name[64];
    char  lltype[64];
    int   is_ptr;
} CGValue;

typedef struct CGSymbol CGSymbol;
struct CGSymbol {
    char      name[64];
    char      lltype[64];
    char      alloca_reg[64];
    CGSymbol *next;
};

typedef struct CGScope CGScope;
struct CGScope {
    CGSymbol *symbols;
    CGScope  *parent;
};

typedef struct {
    FILE     *out;
    int       reg_counter;
    int       label_counter;
    CGScope  *scope;
    int       had_error;
    int       unsafe_depth;
    char    **str_literals;
    int      *str_ids;
    int       str_count;
    char      current_entity[64]; // set when inside entity method codegen
    int       last_was_ret;        // 1 if last stmt in block was a return
} Codegen;

void codegen_init  (Codegen *cg, FILE *out);
void codegen_run   (Codegen *cg, ASTNode *program);
void codegen_free  (Codegen *cg);

#endif /* CODEGEN_H */
