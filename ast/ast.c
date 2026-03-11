#define _GNU_SOURCE
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────
//  ast_new
// ─────────────────────────────────────────────
ASTNode *ast_new(NodeType type, int line, int col) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = type;
    n->line = line;
    n->col  = col;
    return n;
}

// ─────────────────────────────────────────────
//  ast_free  (recursive)
// ─────────────────────────────────────────────
void ast_free(ASTNode *n) {
    if (!n) return;
    int i;
    switch (n->type) {
        case NODE_PROGRAM:
            for (i = 0; i < n->as.program.count; i++) ast_free(n->as.program.stmts[i]);
            free(n->as.program.stmts);
            break;
        case NODE_USE:
            free(n->as.use.path);
            break;
        case NODE_FN_DEF:
            free(n->as.fn_def.name);
            for (i = 0; i < n->as.fn_def.param_count; i++) {
                free(n->as.fn_def.params[i].name);
                ast_free(n->as.fn_def.params[i].type);
            }
            free(n->as.fn_def.params);
            ast_free(n->as.fn_def.return_type);
            ast_free(n->as.fn_def.body);
            break;
        case NODE_ENTITY_DEF:
            free(n->as.entity_def.name);
            for (i = 0; i < n->as.entity_def.member_count; i++)
                ast_free(n->as.entity_def.members[i]);
            free(n->as.entity_def.members);
            break;
        case NODE_LET:
            free(n->as.let.name);
            ast_free(n->as.let.type_node);
            ast_free(n->as.let.init);
            break;
        case NODE_RETURN:
            ast_free(n->as.ret.value);
            break;
        case NODE_IF:
            ast_free(n->as.if_stmt.condition);
            ast_free(n->as.if_stmt.then_block);
            ast_free(n->as.if_stmt.else_block);
            break;
        case NODE_WHILE:
            ast_free(n->as.while_stmt.condition);
            ast_free(n->as.while_stmt.body);
            break;
        case NODE_FOR_RANGE:
            free(n->as.for_range.var);
            ast_free(n->as.for_range.start);
            ast_free(n->as.for_range.end);
            ast_free(n->as.for_range.body);
            break;
        case NODE_FOR_ITER:
            free(n->as.for_iter.var);
            ast_free(n->as.for_iter.iterable);
            ast_free(n->as.for_iter.body);
            break;
        case NODE_UNSAFE_BLOCK:
        case NODE_BLOCK:
            for (i = 0; i < n->as.block.count; i++) ast_free(n->as.block.stmts[i]);
            free(n->as.block.stmts);
            break;
        case NODE_EXPR_STMT:
            /* expression is stored as child program stmts[0] — handled above */
            break;
        case NODE_ASSIGN:
            free(n->as.assign.op);
            ast_free(n->as.assign.target);
            ast_free(n->as.assign.value);
            break;
        case NODE_BINARY:
            free(n->as.binary.op);
            ast_free(n->as.binary.left);
            ast_free(n->as.binary.right);
            break;
        case NODE_UNARY:
            free(n->as.unary.op);
            ast_free(n->as.unary.operand);
            break;
        case NODE_CALL:
            ast_free(n->as.call.callee);
            for (i = 0; i < n->as.call.arg_count; i++) ast_free(n->as.call.args[i]);
            free(n->as.call.args);
            break;
        case NODE_INDEX:
            ast_free(n->as.index_expr.target);
            ast_free(n->as.index_expr.index);
            break;
        case NODE_MEMBER:
            ast_free(n->as.member.object);
            free(n->as.member.field);
            break;
        case NODE_IDENT:       free(n->as.ident.name);       break;
        case NODE_STRING_LIT:  free(n->as.string_lit.value); break;
        case NODE_ARRAY_LIT:
            for (i = 0; i < n->as.array_lit.count; i++) ast_free(n->as.array_lit.elements[i]);
            free(n->as.array_lit.elements);
            break;
        case NODE_TYPE:        free(n->as.type_node.name);   break;
        case NODE_POINTER_TYPE: ast_free(n->as.pointer_type.base); break;
        case NODE_ARRAY_TYPE:  ast_free(n->as.array_type.elem_type); break;
        default: break;
    }
    free(n);
}

// ─────────────────────────────────────────────
//  ast_print  (pretty-print for debugging)
// ─────────────────────────────────────────────
static void indent_print(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

void ast_print(const ASTNode *n, int indent) {
    if (!n) return;
    indent_print(indent);

    switch (n->type) {
        case NODE_PROGRAM:
            printf("Program (%d stmts)\n", n->as.program.count);
            for (int i = 0; i < n->as.program.count; i++)
                ast_print(n->as.program.stmts[i], indent + 1);
            break;
        case NODE_USE:
            printf("Use: %s\n", n->as.use.path);
            break;
        case NODE_FN_DEF:
            printf("FnDef: %s%s (params: %d)\n",
                   n->as.fn_def.is_unsafe ? "unsafe " : "",
                   n->as.fn_def.name,
                   n->as.fn_def.param_count);
            for (int i = 0; i < n->as.fn_def.param_count; i++) {
                indent_print(indent + 1);
                printf("Param: %s : ", n->as.fn_def.params[i].name);
                ast_print(n->as.fn_def.params[i].type, 0);
            }
            if (n->as.fn_def.return_type) {
                indent_print(indent + 1);
                printf("Returns: ");
                ast_print(n->as.fn_def.return_type, 0);
            }
            ast_print(n->as.fn_def.body, indent + 1);
            break;
        case NODE_ENTITY_DEF:
            printf("Entity: %s (%d members)\n",
                   n->as.entity_def.name, n->as.entity_def.member_count);
            for (int i = 0; i < n->as.entity_def.member_count; i++)
                ast_print(n->as.entity_def.members[i], indent + 1);
            break;
        case NODE_LET:
            printf("Let: %s\n", n->as.let.name);
            if (n->as.let.type_node) ast_print(n->as.let.type_node, indent + 1);
            if (n->as.let.init)      ast_print(n->as.let.init,      indent + 1);
            break;
        case NODE_RETURN:
            printf("Return\n");
            ast_print(n->as.ret.value, indent + 1);
            break;
        case NODE_IF:
            printf("If\n");
            indent_print(indent + 1); printf("Condition:\n");
            ast_print(n->as.if_stmt.condition,  indent + 2);
            indent_print(indent + 1); printf("Then:\n");
            ast_print(n->as.if_stmt.then_block, indent + 2);
            if (n->as.if_stmt.else_block) {
                indent_print(indent + 1); printf("Else:\n");
                ast_print(n->as.if_stmt.else_block, indent + 2);
            }
            break;
        case NODE_WHILE:
            printf("While\n");
            ast_print(n->as.while_stmt.condition, indent + 1);
            ast_print(n->as.while_stmt.body,      indent + 1);
            break;
        case NODE_FOR_RANGE:
            printf("ForRange: %s in ..\n", n->as.for_range.var);
            ast_print(n->as.for_range.start, indent + 1);
            ast_print(n->as.for_range.end,   indent + 1);
            ast_print(n->as.for_range.body,  indent + 1);
            break;
        case NODE_FOR_ITER:
            printf("ForIter: %s in\n", n->as.for_iter.var);
            ast_print(n->as.for_iter.iterable, indent + 1);
            ast_print(n->as.for_iter.body,     indent + 1);
            break;
        case NODE_UNSAFE_BLOCK:
            printf("UnsafeBlock (%d stmts)\n", n->as.block.count);
            for (int i = 0; i < n->as.block.count; i++)
                ast_print(n->as.block.stmts[i], indent + 1);
            break;
        case NODE_BLOCK:
            printf("Block (%d stmts)\n", n->as.block.count);
            for (int i = 0; i < n->as.block.count; i++)
                ast_print(n->as.block.stmts[i], indent + 1);
            break;
        case NODE_EXPR_STMT:
            printf("ExprStmt\n");
            break;
        case NODE_ASSIGN:
            printf("Assign: %s\n", n->as.assign.op);
            ast_print(n->as.assign.target, indent + 1);
            ast_print(n->as.assign.value,  indent + 1);
            break;
        case NODE_BINARY:
            printf("Binary: %s\n", n->as.binary.op);
            ast_print(n->as.binary.left,  indent + 1);
            ast_print(n->as.binary.right, indent + 1);
            break;
        case NODE_UNARY:
            printf("Unary: %s\n", n->as.unary.op);
            ast_print(n->as.unary.operand, indent + 1);
            break;
        case NODE_CALL:
            printf("Call (args: %d)\n", n->as.call.arg_count);
            ast_print(n->as.call.callee, indent + 1);
            for (int i = 0; i < n->as.call.arg_count; i++)
                ast_print(n->as.call.args[i], indent + 1);
            break;
        case NODE_INDEX:
            printf("Index\n");
            ast_print(n->as.index_expr.target, indent + 1);
            ast_print(n->as.index_expr.index,  indent + 1);
            break;
        case NODE_MEMBER:
            printf("Member: .%s\n", n->as.member.field);
            ast_print(n->as.member.object, indent + 1);
            break;
        case NODE_IDENT:       printf("Ident: %s\n",      n->as.ident.name);            break;
        case NODE_INT_LIT:     printf("Int: %lld\n",      n->as.int_lit.value);          break;
        case NODE_FLOAT_LIT:   printf("Float: %g\n",      n->as.float_lit.value);        break;
        case NODE_STRING_LIT:  printf("String: \"%s\"\n", n->as.string_lit.value);       break;
        case NODE_CHAR_LIT:    printf("Char: '%c'\n",     n->as.char_lit.value);         break;
        case NODE_BOOL_LIT:    printf("Bool: %s\n",       n->as.bool_lit.value ? "true" : "false"); break;
        case NODE_ARRAY_LIT:
            printf("ArrayLit (%d elements)\n", n->as.array_lit.count);
            for (int i = 0; i < n->as.array_lit.count; i++)
                ast_print(n->as.array_lit.elements[i], indent + 1);
            break;
        case NODE_TYPE:         printf("Type: %s\n",  n->as.type_node.name);   break;
        case NODE_POINTER_TYPE:
            printf("PointerType\n");
            ast_print(n->as.pointer_type.base, indent + 1);
            break;
        case NODE_ARRAY_TYPE:
            printf("ArrayType (size: %d)\n", n->as.array_type.size);
            ast_print(n->as.array_type.elem_type, indent + 1);
            break;
        default:
            printf("Unknown node\n");
    }
}