#define _GNU_SOURCE
#include "analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Error reporting
// ─────────────────────────────────────────────
static void sem_error(Analyzer *a, const ASTNode *node, const char *msg) {
    a->had_error = 1;
    fprintf(stderr, "[line %d:%d] SemanticError: %s\n",
            node ? node->line : 0,
            node ? node->col  : 0,
            msg);
}

// ─────────────────────────────────────────────
//  TypeInfo helpers
// ─────────────────────────────────────────────
TypeInfo *type_new(TaiType kind) {
    TypeInfo *t = calloc(1, sizeof(TypeInfo));
    t->kind = kind;
    t->array_size = -1;
    return t;
}

void type_free(TypeInfo *t) {
    if (!t) return;
    type_free(t->ptr_base);
    type_free(t->elem);
    free(t->entity_name);
    free(t);
}

int type_equal(const TypeInfo *a, const TypeInfo *b) {
    if (!a || !b) return a == b;
    if (a->kind != b->kind) return 0;
    if (a->kind == TY_POINTER) {
        // unknown base means any pointer matches
        if (!a->ptr_base || !b->ptr_base) return 1;
        if (a->ptr_base->kind == TY_UNKNOWN || b->ptr_base->kind == TY_UNKNOWN) return 1;
        return type_equal(a->ptr_base, b->ptr_base);
    }
    if (a->kind == TY_ARRAY) {
        if (!a->elem || !b->elem) return 1;
        if (a->elem->kind == TY_UNKNOWN || b->elem->kind == TY_UNKNOWN) return 1;
        return type_equal(a->elem, b->elem);
    }
    if (a->kind == TY_ENTITY)  return strcmp(a->entity_name, b->entity_name) == 0;
    return 1;
}

int type_numeric(const TypeInfo *t) {
    if (!t) return 0;
    switch (t->kind) {
        case TY_I8: case TY_I16: case TY_I32: case TY_I64:
        case TY_U8: case TY_U16: case TY_U32: case TY_U64:
        case TY_F32: case TY_F64:
            return 1;
        default: return 0;
    }
}

int type_integer(const TypeInfo *t) {
    if (!t) return 0;
    switch (t->kind) {
        case TY_I8: case TY_I16: case TY_I32: case TY_I64:
        case TY_U8: case TY_U16: case TY_U32: case TY_U64:
            return 1;
        default: return 0;
    }
}

const char *type_name(const TypeInfo *t) {
    if (!t) return "null";
    switch (t->kind) {
        case TY_VOID:    return "void";
        case TY_I8:      return "i8";
        case TY_I16:     return "i16";
        case TY_I32:     return "i32";
        case TY_I64:     return "i64";
        case TY_U8:      return "u8";
        case TY_U16:     return "u16";
        case TY_U32:     return "u32";
        case TY_U64:     return "u64";
        case TY_F32:     return "f32";
        case TY_F64:     return "f64";
        case TY_BOOL:    return "bool";
        case TY_STR:     return "str";
        case TY_CHAR:    return "char";
        case TY_TENSOR:  return "tensor";
        case TY_ANY:     return "any";
        case TY_POINTER: return "pointer";
        case TY_ARRAY:   return "array";
        case TY_ENTITY:  return t->entity_name ? t->entity_name : "entity";
        case TY_FN:      return "fn";
        default:         return "unknown";
    }
}

// Convert AST type node → TypeInfo
TypeInfo *type_from_node(ASTNode *node) {
    if (!node) return type_new(TY_VOID);
    if (node->type == NODE_POINTER_TYPE) {
        TypeInfo *t = type_new(TY_POINTER);
        t->ptr_base = type_from_node(node->as.pointer_type.base);
        return t;
    }
    if (node->type == NODE_ARRAY_TYPE) {
        TypeInfo *t = type_new(TY_ARRAY);
        t->elem       = type_from_node(node->as.array_type.elem_type);
        t->array_size = node->as.array_type.size;
        return t;
    }
    // NODE_TYPE — map name string → TaiType
    const char *name = node->as.type_node.name;
    if (!name) return type_new(TY_UNKNOWN);

    if (strcmp(name, "i8")     == 0) return type_new(TY_I8);
    if (strcmp(name, "i16")    == 0) return type_new(TY_I16);
    if (strcmp(name, "i32")    == 0) return type_new(TY_I32);
    if (strcmp(name, "i64")    == 0) return type_new(TY_I64);
    if (strcmp(name, "u8")     == 0) return type_new(TY_U8);
    if (strcmp(name, "u16")    == 0) return type_new(TY_U16);
    if (strcmp(name, "u32")    == 0) return type_new(TY_U32);
    if (strcmp(name, "u64")    == 0) return type_new(TY_U64);
    if (strcmp(name, "f32")    == 0) return type_new(TY_F32);
    if (strcmp(name, "f64")    == 0) return type_new(TY_F64);
    if (strcmp(name, "bool")   == 0) return type_new(TY_BOOL);
    if (strcmp(name, "str")    == 0) return type_new(TY_STR);
    if (strcmp(name, "char")   == 0) return type_new(TY_CHAR);
    if (strcmp(name, "tensor") == 0) return type_new(TY_TENSOR);
    if (strcmp(name, "any")    == 0) return type_new(TY_ANY);

    // Assume entity name
    TypeInfo *t = type_new(TY_ENTITY);
    t->entity_name = strdup(name);
    return t;
}

// ─────────────────────────────────────────────
//  Scope management
// ─────────────────────────────────────────────
static Scope *scope_push(Analyzer *a) {
    Scope *s = calloc(1, sizeof(Scope));
    s->parent = a->scope;
    a->scope  = s;
    return s;
}

static void scope_pop(Analyzer *a) {
    if (!a->scope) return;
    Scope *old = a->scope;
    a->scope   = old->parent;
    // free symbols
    Symbol *sym = old->symbols;
    while (sym) {
        Symbol *next = sym->next;
        free(sym->name);
        type_free(sym->type);
        for (int i = 0; i < sym->param_count; i++)
            type_free(sym->param_types[i]);
        free(sym->param_types);
        type_free(sym->return_type);
        free(sym);
        sym = next;
    }
    free(old);
}

static void scope_define(Analyzer *a, const char *name, TypeInfo *type) {
    Symbol *sym     = calloc(1, sizeof(Symbol));
    sym->name       = strdup(name);
    sym->type       = type;
    sym->next       = a->scope->symbols;
    a->scope->symbols = sym;
}

static void scope_define_fn(Analyzer *a, const char *name,
                             TypeInfo **param_types, int param_count,
                             TypeInfo *return_type) {
    Symbol *sym       = calloc(1, sizeof(Symbol));
    sym->name         = strdup(name);
    sym->type         = type_new(TY_FN);
    sym->is_fn        = 1;
    sym->param_count  = param_count;
    sym->param_types  = param_types;
    sym->return_type  = return_type;
    sym->next         = a->scope->symbols;
    a->scope->symbols = sym;
}

static Symbol *scope_lookup(Analyzer *a, const char *name) {
    for (Scope *s = a->scope; s; s = s->parent) {
        for (Symbol *sym = s->symbols; sym; sym = sym->next)
            if (strcmp(sym->name, name) == 0)
                return sym;
    }
    return NULL;
}

// ─────────────────────────────────────────────
//  Forward declaration
// ─────────────────────────────────────────────
static TypeInfo *analyze_expr  (Analyzer *a, ASTNode *node);
static void      analyze_stmt  (Analyzer *a, ASTNode *node);
static void      analyze_block (Analyzer *a, ASTNode *node);

// ─────────────────────────────────────────────
//  Expression analysis — returns resolved type
// ─────────────────────────────────────────────
static TypeInfo *analyze_expr(Analyzer *a, ASTNode *node) {
    if (!node) return type_new(TY_VOID);

    switch (node->type) {

        case NODE_INT_LIT:    return type_new(TY_I32);
        case NODE_FLOAT_LIT:  return type_new(TY_F32);
        case NODE_STRING_LIT: return type_new(TY_STR);
        case NODE_CHAR_LIT:   return type_new(TY_CHAR);
        case NODE_BOOL_LIT:   return type_new(TY_BOOL);

        case NODE_ARRAY_LIT: {
            TypeInfo *t = type_new(TY_ARRAY);
            t->array_size = node->as.array_lit.count;
            if (node->as.array_lit.count > 0)
                t->elem = analyze_expr(a, node->as.array_lit.elements[0]);
            else
                t->elem = type_new(TY_UNKNOWN);
            // analyze remaining elements
            for (int i = 1; i < node->as.array_lit.count; i++)
                type_free(analyze_expr(a, node->as.array_lit.elements[i]));
            return t;
        }

        case NODE_IDENT: {
            Symbol *sym = scope_lookup(a, node->as.ident.name);
            if (!sym) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Undefined variable '%s'", node->as.ident.name);
                sem_error(a, node, msg);
                return type_new(TY_UNKNOWN);
            }
            // return a copy
            TypeInfo *copy = type_new(sym->type->kind);
            copy->array_size  = sym->type->array_size;
            if (sym->type->entity_name) copy->entity_name = strdup(sym->type->entity_name);
            if (sym->type->elem) {
                copy->elem = type_new(sym->type->elem->kind);
                copy->elem->array_size = sym->type->elem->array_size;
                if (sym->type->elem->entity_name) copy->elem->entity_name = strdup(sym->type->elem->entity_name);
            }
            if (sym->type->ptr_base) {
                copy->ptr_base = type_new(sym->type->ptr_base->kind);
            }
            return copy;
        }

        case NODE_UNARY: {
            TypeInfo *operand = analyze_expr(a, node->as.unary.operand);
            const char *op = node->as.unary.op;
            if (strcmp(op, "-") == 0 && !type_numeric(operand)) {
                sem_error(a, node, "Unary '-' requires numeric type");
            }
            if (strcmp(op, "!") == 0 && operand->kind != TY_BOOL) {
                sem_error(a, node, "Unary '!' requires bool type");
            }
            return operand;
        }

        case NODE_BINARY: {
            TypeInfo *left  = analyze_expr(a, node->as.binary.left);
            TypeInfo *right = analyze_expr(a, node->as.binary.right);
            const char *op  = node->as.binary.op;

            // Comparison ops always return bool
            if (strcmp(op,"==")==0 || strcmp(op,"!=")==0 ||
                strcmp(op,"<") ==0 || strcmp(op,">")==0  ||
                strcmp(op,"<=")==0 || strcmp(op,">=")==0) {
                type_free(left); type_free(right);
                return type_new(TY_BOOL);
            }
            // Logical ops
            if (strcmp(op,"&")==0 || strcmp(op,"|")==0) {
                type_free(left); type_free(right);
                return type_new(TY_BOOL);
            }
            // Arithmetic: both sides must be numeric
            if (!type_numeric(left) || !type_numeric(right)) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Operator '%s' requires numeric operands (got %s and %s)",
                         op, type_name(left), type_name(right));
                sem_error(a, node, msg);
            }
            // Result type = left type (Taipan uses left-hand promotion)
            type_free(right);
            return left;
        }

        case NODE_ASSIGN: {
            TypeInfo *val = analyze_expr(a, node->as.assign.value);
            TypeInfo *tgt = analyze_expr(a, node->as.assign.target);
            const char *op = node->as.assign.op;
            // compound assign ops need numeric types
            if (strcmp(op, "=") != 0 && (!type_numeric(tgt) || !type_numeric(val))) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Compound assignment '%s' requires numeric types", op);
                sem_error(a, node, msg);
            }
            type_free(val);
            return tgt;
        }

        case NODE_INDEX: {
            TypeInfo *target = analyze_expr(a, node->as.index_expr.target);
            TypeInfo *idx    = analyze_expr(a, node->as.index_expr.index);
            if (!type_integer(idx)) {
                sem_error(a, node, "Array index must be an integer type");
            }
            type_free(idx);
            // unsafe check: pointer indexing requires unsafe context
            if (target->kind == TY_POINTER && a->unsafe_depth == 0) {
                sem_error(a, node, "Pointer indexing requires an unsafe block");
            }
            TypeInfo *elem_type;
            if (target->kind == TY_ARRAY && target->elem)
                elem_type = type_new(target->elem->kind);
            else if (target->kind == TY_POINTER && target->ptr_base)
                elem_type = type_new(target->ptr_base->kind);
            else
                elem_type = type_new(TY_UNKNOWN);
            type_free(target);
            return elem_type;
        }

        case NODE_MEMBER: {
            // Resolve object type — for now return TY_UNKNOWN (full resolution needs entity table)
            type_free(analyze_expr(a, node->as.member.object));
            return type_new(TY_UNKNOWN);
        }

        case NODE_CALL: {
            // Check callee is defined
            if (node->as.call.callee->type == NODE_IDENT) {
                const char *fname = node->as.call.callee->as.ident.name;
                Symbol *sym = scope_lookup(a, fname);

                // Built-in functions: alloc / free require unsafe
                if ((strcmp(fname, "alloc") == 0 || strcmp(fname, "free") == 0)
                    && a->unsafe_depth == 0) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Call to '%s' requires an unsafe block", fname);
                    sem_error(a, node, msg);
                }

                // Analyze all args
                // Named args (x=val) are NODE_ASSIGN — only check value side
                for (int i = 0; i < node->as.call.arg_count; i++) {
                    ASTNode *arg = node->as.call.args[i];
                    if (arg->type == NODE_ASSIGN)
                        type_free(analyze_expr(a, arg->as.assign.value));
                    else
                        type_free(analyze_expr(a, arg));
                }

                // Check arity if symbol is known fn
                if (sym && sym->is_fn && sym->param_count != node->as.call.arg_count) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Function '%s' expects %d args but got %d",
                             fname, sym->param_count, node->as.call.arg_count);
                    sem_error(a, node, msg);
                }

                if (sym && sym->is_fn && sym->return_type)
                    return type_new(sym->return_type->kind);
            } else {
                // Method call or complex callee — analyze callee + args
                type_free(analyze_expr(a, node->as.call.callee));
                for (int i = 0; i < node->as.call.arg_count; i++)
                    type_free(analyze_expr(a, node->as.call.args[i]));
            }
            return type_new(TY_UNKNOWN); // return type unknown without full entity resolution
        }

        default:
            return type_new(TY_UNKNOWN);
    }
}

// ─────────────────────────────────────────────
//  Block analysis
// ─────────────────────────────────────────────
static void analyze_block(Analyzer *a, ASTNode *node) {
    if (!node) return;
    scope_push(a);
    for (int i = 0; i < node->as.block.count; i++)
        analyze_stmt(a, node->as.block.stmts[i]);
    scope_pop(a);
}

// ─────────────────────────────────────────────
//  Statement analysis
// ─────────────────────────────────────────────
static void analyze_stmt(Analyzer *a, ASTNode *node) {
    if (!node) return;

    switch (node->type) {

        case NODE_USE:
            // Nothing to check at this stage — import resolution is later
            break;

        case NODE_ENTITY_DEF: {
            // Register entity name as a type in scope
            TypeInfo *et = type_new(TY_ENTITY);
            et->entity_name = strdup(node->as.entity_def.name);
            scope_define(a, node->as.entity_def.name, et);

            // Collect fields first
            int fcount = 0;
            char *fnames[64]; TypeInfo *ftypes[64];
            for (int i = 0; i < node->as.entity_def.member_count; i++) {
                ASTNode *m = node->as.entity_def.members[i];
                if (m->type == NODE_LET && m->as.let.type_node && fcount < 64) {
                    fnames[fcount] = m->as.let.name;
                    ftypes[fcount] = type_from_node(m->as.let.type_node);
                    fcount++;
                }
            }
            // Analyze each method with fields pre-injected into scope
            for (int i = 0; i < node->as.entity_def.member_count; i++) {
                ASTNode *m = node->as.entity_def.members[i];
                if (m->type != NODE_FN_DEF) continue;

                int pc = m->as.fn_def.param_count;
                TypeInfo **ptypes2 = calloc(pc, sizeof(TypeInfo*));
                for (int pi = 0; pi < pc; pi++)
                    ptypes2[pi] = type_from_node(m->as.fn_def.params[pi].type);
                TypeInfo *ret2 = type_from_node(m->as.fn_def.return_type);
                // Register method as EntityName__method
                char mangled[128];
                snprintf(mangled, sizeof(mangled), "%s__%s",
                         node->as.entity_def.name, m->as.fn_def.name);
                scope_define_fn(a, mangled, ptypes2, pc, ret2);

                scope_push(a);
                // Inject entity fields as local variables (fresh copies per method)
                for (int fi = 0; fi < fcount; fi++) {
                    TypeInfo *fc = type_new(ftypes[fi]->kind);
                    if (ftypes[fi]->entity_name) fc->entity_name = strdup(ftypes[fi]->entity_name);
                    scope_define(a, fnames[fi], fc);
                }
                // Inject params
                for (int pi = 0; pi < pc; pi++) {
                    TypeInfo *pt = type_from_node(m->as.fn_def.params[pi].type);
                    scope_define(a, m->as.fn_def.params[pi].name, pt);
                }
                TypeInfo *prev_ret = a->current_return_type;
                TypeInfo *ret2_copy = type_new(ret2->kind);
                if (ret2->entity_name) ret2_copy->entity_name = strdup(ret2->entity_name);
                a->current_return_type = ret2_copy;
                if (m->as.fn_def.is_unsafe) a->unsafe_depth++;
                if (m->as.fn_def.body) {
                    for (int si = 0; si < m->as.fn_def.body->as.block.count; si++)
                        analyze_stmt(a, m->as.fn_def.body->as.block.stmts[si]);
                }
                if (m->as.fn_def.is_unsafe) a->unsafe_depth--;
                type_free(a->current_return_type);
                a->current_return_type = prev_ret;
                scope_pop(a);
                // ptypes2 is owned by scope_define_fn, freed by scope_pop
            }
            break;
        }

        case NODE_FN_DEF: {
            // Build param types for symbol registration
            int pc = node->as.fn_def.param_count;
            TypeInfo **ptypes = calloc(pc, sizeof(TypeInfo*));
            for (int i = 0; i < pc; i++)
                ptypes[i] = type_from_node(node->as.fn_def.params[i].type);

            TypeInfo *ret = type_from_node(node->as.fn_def.return_type);
            scope_define_fn(a, node->as.fn_def.name, ptypes, pc, ret);

            // Analyze body in new scope with params registered
            scope_push(a);
            for (int i = 0; i < pc; i++) {
                TypeInfo *pt = type_from_node(node->as.fn_def.params[i].type);
                scope_define(a, node->as.fn_def.params[i].name, pt);
            }

            TypeInfo *prev_ret = a->current_return_type;
            a->current_return_type = type_from_node(node->as.fn_def.return_type);

            if (node->as.fn_def.is_unsafe) a->unsafe_depth++;
            if (node->as.fn_def.body) {
                for (int i = 0; i < node->as.fn_def.body->as.block.count; i++)
                    analyze_stmt(a, node->as.fn_def.body->as.block.stmts[i]);
            }
            if (node->as.fn_def.is_unsafe) a->unsafe_depth--;

            type_free(a->current_return_type);
            a->current_return_type = prev_ret;
            scope_pop(a);
            break;
        }

        case NODE_LET: {
            TypeInfo *declared = node->as.let.type_node
                                 ? type_from_node(node->as.let.type_node)
                                 : NULL;
            TypeInfo *inferred = node->as.let.init
                                 ? analyze_expr(a, node->as.let.init)
                                 : NULL;

            // Check type mismatch if both are known
            if (declared && inferred &&
                declared->kind != TY_UNKNOWN && inferred->kind != TY_UNKNOWN &&
                declared->kind != TY_ANY) {
                if (!type_equal(declared, inferred)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Type mismatch in 'let %s': declared %s but got %s",
                             node->as.let.name,
                             type_name(declared), type_name(inferred));
                    sem_error(a, node, msg);
                }
            }

            TypeInfo *final_type = declared ? declared : (inferred ? inferred : type_new(TY_UNKNOWN));
            if (declared && inferred && declared != inferred) type_free(inferred);
            scope_define(a, node->as.let.name, final_type);
            break;
        }

        case NODE_RETURN: {
            TypeInfo *val_type = analyze_expr(a, node->as.ret.value);
            if (a->current_return_type &&
                a->current_return_type->kind != TY_VOID &&
                a->current_return_type->kind != TY_UNKNOWN &&
                val_type->kind != TY_UNKNOWN) {
                if (!type_equal(a->current_return_type, val_type)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Return type mismatch: expected %s but got %s",
                             type_name(a->current_return_type), type_name(val_type));
                    sem_error(a, node, msg);
                }
            }
            type_free(val_type);
            break;
        }

        case NODE_IF: {
            TypeInfo *cond = analyze_expr(a, node->as.if_stmt.condition);
            if (cond->kind != TY_BOOL && cond->kind != TY_UNKNOWN) {
                sem_error(a, node, "If condition must be a bool expression");
            }
            type_free(cond);
            analyze_block(a, node->as.if_stmt.then_block);
            if (node->as.if_stmt.else_block) {
                if (node->as.if_stmt.else_block->type == NODE_IF)
                    analyze_stmt(a, node->as.if_stmt.else_block);
                else
                    analyze_block(a, node->as.if_stmt.else_block);
            }
            break;
        }

        case NODE_WHILE: {
            TypeInfo *cond = analyze_expr(a, node->as.while_stmt.condition);
            if (cond->kind != TY_BOOL && cond->kind != TY_UNKNOWN) {
                sem_error(a, node, "While condition must be a bool expression");
            }
            type_free(cond);
            analyze_block(a, node->as.while_stmt.body);
            break;
        }

        case NODE_FOR_RANGE: {
            TypeInfo *start = analyze_expr(a, node->as.for_range.start);
            TypeInfo *end   = analyze_expr(a, node->as.for_range.end);
            if (!type_integer(start))
                sem_error(a, node, "For range start must be an integer");
            if (!type_integer(end))
                sem_error(a, node, "For range end must be an integer");
            type_free(start); type_free(end);

            scope_push(a);
            scope_define(a, node->as.for_range.var, type_new(TY_I32));
            for (int i = 0; i < node->as.for_range.body->as.block.count; i++)
                analyze_stmt(a, node->as.for_range.body->as.block.stmts[i]);
            scope_pop(a);
            break;
        }

        case NODE_FOR_ITER: {
            TypeInfo *iter = analyze_expr(a, node->as.for_iter.iterable);
            TypeInfo *elem = (iter->kind == TY_ARRAY && iter->elem)
                             ? type_new(iter->elem->kind)
                             : type_new(TY_UNKNOWN);
            type_free(iter);

            scope_push(a);
            scope_define(a, node->as.for_iter.var, elem);
            for (int i = 0; i < node->as.for_iter.body->as.block.count; i++)
                analyze_stmt(a, node->as.for_iter.body->as.block.stmts[i]);
            scope_pop(a);
            break;
        }

        case NODE_UNSAFE_BLOCK: {
            a->unsafe_depth++;
            scope_push(a);
            for (int i = 0; i < node->as.block.count; i++)
                analyze_stmt(a, node->as.block.stmts[i]);
            scope_pop(a);
            a->unsafe_depth--;
            break;
        }

        case NODE_BLOCK:
            analyze_block(a, node);
            break;

        default:
            // Expression statement
            type_free(analyze_expr(a, node));
            break;
    }
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────
void analyzer_init(Analyzer *a) {
    memset(a, 0, sizeof(Analyzer));
    // Push global scope
    a->scope = calloc(1, sizeof(Scope));

    // Register built-in functions
    // println(any) -> void
    TypeInfo **println_params = calloc(1, sizeof(TypeInfo*));
    println_params[0] = type_new(TY_ANY);
    scope_define_fn(a, "println", println_params, 1, type_new(TY_VOID));

    // print(any) -> void
    TypeInfo **print_params = calloc(1, sizeof(TypeInfo*));
    print_params[0] = type_new(TY_ANY);
    scope_define_fn(a, "print", print_params, 1, type_new(TY_VOID));

    // alloc(i32) -> pointer  [unsafe]
    TypeInfo **alloc_params = calloc(1, sizeof(TypeInfo*));
    alloc_params[0] = type_new(TY_I32);
    TypeInfo *alloc_ret = type_new(TY_POINTER);
    alloc_ret->ptr_base = type_new(TY_UNKNOWN);
    scope_define_fn(a, "alloc", alloc_params, 1, alloc_ret);

    // free(pointer) -> void  [unsafe]
    TypeInfo **free_params = calloc(1, sizeof(TypeInfo*));
    free_params[0] = type_new(TY_POINTER);
    scope_define_fn(a, "free", free_params, 1, type_new(TY_VOID));

    // len(array) -> i32
    TypeInfo **len_params = calloc(1, sizeof(TypeInfo*));
    len_params[0] = type_new(TY_ARRAY);
    scope_define_fn(a, "len", len_params, 1, type_new(TY_I32));
}

void analyzer_run(Analyzer *a, ASTNode *program) {
    if (!program || program->type != NODE_PROGRAM) return;

    // First pass: register all top-level fn/entity names
    for (int i = 0; i < program->as.program.count; i++) {
        ASTNode *n = program->as.program.stmts[i];
        if (n->type == NODE_FN_DEF) {
            int pc = n->as.fn_def.param_count;
            TypeInfo **ptypes = calloc(pc, sizeof(TypeInfo*));
            for (int j = 0; j < pc; j++)
                ptypes[j] = type_from_node(n->as.fn_def.params[j].type);
            TypeInfo *ret = type_from_node(n->as.fn_def.return_type);
            scope_define_fn(a, n->as.fn_def.name, ptypes, pc, ret);
        }
        if (n->type == NODE_ENTITY_DEF) {
            TypeInfo *et = type_new(TY_ENTITY);
            et->entity_name = strdup(n->as.entity_def.name);
            scope_define(a, n->as.entity_def.name, et);
        }
    }

    // Second pass: full analysis
    for (int i = 0; i < program->as.program.count; i++)
        analyze_stmt(a, program->as.program.stmts[i]);
}

void analyzer_free(Analyzer *a) {
    while (a->scope) scope_pop(a);
}
