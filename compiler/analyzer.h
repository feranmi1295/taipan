#ifndef ANALYZER_H
#define ANALYZER_H

#include "../ast/ast.h"

// ─────────────────────────────────────────────
//  Resolved Type System
// ─────────────────────────────────────────────
typedef enum {
    TY_UNKNOWN,
    TY_VOID,
    TY_I8,  TY_I16, TY_I32, TY_I64,
    TY_U8,  TY_U16, TY_U32, TY_U64,
    TY_F32, TY_F64,
    TY_BOOL, TY_STR, TY_CHAR, TY_TENSOR, TY_ANY,
    TY_POINTER,     // ptr_base = pointee type
    TY_ARRAY,       // elem + array_size (-1 = dynamic)
    TY_ENTITY,      // entity_name = name string
    TY_FN,
} TaiType;

typedef struct TypeInfo TypeInfo;
struct TypeInfo {
    TaiType   kind;
    TypeInfo *ptr_base;
    TypeInfo *elem;
    int       array_size;
    char     *entity_name;
};

// ─────────────────────────────────────────────
//  Symbol Table
// ─────────────────────────────────────────────
typedef struct Symbol Symbol;
struct Symbol {
    char      *name;
    TypeInfo  *type;
    int        is_fn;
    int        param_count;
    TypeInfo **param_types;
    TypeInfo  *return_type;
    Symbol    *next;
};

typedef struct Scope Scope;
struct Scope {
    Symbol *symbols;
    Scope  *parent;
};

// ─────────────────────────────────────────────
//  Analyzer
// ─────────────────────────────────────────────
typedef struct {
    Scope    *scope;
    int       had_error;
    int       unsafe_depth;
    TypeInfo *current_return_type;
} Analyzer;

// ─────────────────────────────────────────────
//  API
// ─────────────────────────────────────────────
void        analyzer_init    (Analyzer *a);
void        analyzer_run     (Analyzer *a, ASTNode *program);
void        analyzer_free    (Analyzer *a);

TypeInfo   *type_new         (TaiType kind);
TypeInfo   *type_from_node   (ASTNode *node);
int         type_equal       (const TypeInfo *a, const TypeInfo *b);
int         type_numeric     (const TypeInfo *t);
int         type_integer     (const TypeInfo *t);
const char *type_name        (const TypeInfo *t);
void        type_free        (TypeInfo *t);

#endif /* ANALYZER_H */