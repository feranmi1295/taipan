#define _GNU_SOURCE
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────

static void error_at(Parser *p, const Token *tok, const char *msg) {
    if (p->panic_mode) return;
    p->panic_mode = 1;
    p->had_error  = 1;
    fprintf(stderr, "[line %d:%d] ParseError: %s (got `%.*s`)\n",
            tok->line, tok->col, msg, tok->length, tok->start);
}

static void advance(Parser *p) {
    p->previous = p->current;
    // skip newlines and comments
    while (1) {
        p->current = lexer_next_token(p->lexer);
        if (p->current.type != TOK_NEWLINE &&
            p->current.type != TOK_COMMENT)
            break;
    }
}

static int check(Parser *p, TokenType t) {
    return p->current.type == t;
}

static int match(Parser *p, TokenType t) {
    if (!check(p, t)) return 0;
    advance(p);
    return 1;
}

static Token consume(Parser *p, TokenType t, const char *msg) {
    if (p->current.type == t) { advance(p); return p->previous; }
    error_at(p, &p->current, msg);
    return p->current;
}

// strndup helper (POSIX, safe copy)
static char *tok_str(const Token *tok) {
    char *s = malloc(tok->length + 1);
    memcpy(s, tok->start, tok->length);
    s[tok->length] = '\0';
    return s;
}

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
static ASTNode *parse_stmt      (Parser *p);
static ASTNode *parse_block     (Parser *p);
static ASTNode *parse_expr      (Parser *p);
static ASTNode *parse_type      (Parser *p);

// ─────────────────────────────────────────────
//  Type parsing
//  i32 | f32 | str | bool | ... | i32* | [i32] | [i32;5]
// ─────────────────────────────────────────────
static int is_type_token(TokenType t) {
    return t == TOK_I8  || t == TOK_I16 || t == TOK_I32 || t == TOK_I64 ||
           t == TOK_U8  || t == TOK_U16 || t == TOK_U32 || t == TOK_U64 ||
           t == TOK_F32 || t == TOK_F64 ||
           t == TOK_BOOL_TYPE || t == TOK_STR_TYPE ||
           t == TOK_CHAR_TYPE || t == TOK_TENSOR_TYPE ||
           t == TOK_IDENT;
}

static ASTNode *parse_type(Parser *p) {
    int line = p->current.line, col = p->current.col;

    // Array type: [i32] or [i32;5]
    if (match(p, TOK_LBRACKET)) {
        ASTNode *elem = parse_type(p);
        int size = -1;
        if (match(p, TOK_SEMICOLON)) {
            Token t = consume(p, TOK_INT_LIT, "Expected array size");
            size = (int)t.value.int_val;
        }
        consume(p, TOK_RBRACKET, "Expected ']' after array type");
        ASTNode *n = ast_new(NODE_ARRAY_TYPE, line, col);
        n->as.array_type.elem_type = elem;
        n->as.array_type.size      = size;
        return n;
    }

    // Base type (keyword or ident)
    if (!is_type_token(p->current.type)) {
        error_at(p, &p->current, "Expected type");
        return NULL;
    }
    advance(p);
    ASTNode *n = ast_new(NODE_TYPE, line, col);
    n->as.type_node.name = tok_str(&p->previous);

    // Pointer: i32*
    if (match(p, TOK_STAR)) {
        ASTNode *ptr = ast_new(NODE_POINTER_TYPE, line, col);
        ptr->as.pointer_type.base = n;
        return ptr;
    }
    return n;
}

// ─────────────────────────────────────────────
//  Expression parsing  (Pratt / precedence climb)
// ─────────────────────────────────────────────

static ASTNode *parse_primary(Parser *p) {
    int line = p->current.line, col = p->current.col;

    // Integer literal
    if (match(p, TOK_INT_LIT)) {
        ASTNode *n = ast_new(NODE_INT_LIT, line, col);
        n->as.int_lit.value = p->previous.value.int_val;
        return n;
    }
    // Float literal
    if (match(p, TOK_FLOAT_LIT)) {
        ASTNode *n = ast_new(NODE_FLOAT_LIT, line, col);
        n->as.float_lit.value = p->previous.value.float_val;
        return n;
    }
    // String literal
    if (match(p, TOK_STRING_LIT)) {
        ASTNode *n = ast_new(NODE_STRING_LIT, line, col);
        // strip surrounding quotes
        char *raw = tok_str(&p->previous);
        int   len = (int)strlen(raw);
        char *inner = malloc(len);
        memcpy(inner, raw + 1, len - 2);
        inner[len - 2] = '\0';
        free(raw);
        n->as.string_lit.value = inner;
        return n;
    }
    // Char literal
    if (match(p, TOK_CHAR_LIT)) {
        ASTNode *n = ast_new(NODE_CHAR_LIT, line, col);
        n->as.char_lit.value = p->previous.value.char_val;
        return n;
    }
    // Bool literal
    if (match(p, TOK_BOOL_LIT)) {
        ASTNode *n = ast_new(NODE_BOOL_LIT, line, col);
        n->as.bool_lit.value = p->previous.value.bool_val;
        return n;
    }
    // Array literal: [1, 2, 3]
    if (match(p, TOK_LBRACKET)) {
        ASTNode *n = ast_new(NODE_ARRAY_LIT, line, col);
        ASTNode **elems = NULL;
        int count = 0;
        if (!check(p, TOK_RBRACKET)) {
            do {
                elems = realloc(elems, sizeof(ASTNode*) * (count + 1));
                elems[count++] = parse_expr(p);
            } while (match(p, TOK_COMMA));
        }
        consume(p, TOK_RBRACKET, "Expected ']'");
        n->as.array_lit.elements = elems;
        n->as.array_lit.count    = count;
        return n;
    }
    // Grouped expression: (expr)
    if (match(p, TOK_LPAREN)) {
        ASTNode *n = parse_expr(p);
        consume(p, TOK_RPAREN, "Expected ')'");
        return n;
    }
    // Identifier (may become call, index, member)
    if (match(p, TOK_IDENT)) {
        ASTNode *n = ast_new(NODE_IDENT, line, col);
        n->as.ident.name = tok_str(&p->previous);
        return n;
    }

    error_at(p, &p->current, "Expected expression");
    advance(p);
    return ast_new(NODE_INT_LIT, line, col); // error recovery placeholder
}

static ASTNode *parse_postfix(Parser *p) {
    ASTNode *expr = parse_primary(p);

    while (1) {
        int line = p->current.line, col = p->current.col;

        // Function call:  expr(args...)
        if (match(p, TOK_LPAREN)) {
            ASTNode *call = ast_new(NODE_CALL, line, col);
            call->as.call.callee    = expr;
            call->as.call.args      = NULL;
            call->as.call.arg_count = 0;

            if (!check(p, TOK_RPAREN)) {
                do {
                    // named args: x=0.0 — parse as assign expr
                    call->as.call.args = realloc(call->as.call.args,
                        sizeof(ASTNode*) * (call->as.call.arg_count + 1));
                    call->as.call.args[call->as.call.arg_count++] = parse_expr(p);
                } while (match(p, TOK_COMMA));
            }
            consume(p, TOK_RPAREN, "Expected ')' after arguments");
            expr = call;
            continue;
        }
        // Index:  expr[index]
        if (match(p, TOK_LBRACKET)) {
            ASTNode *idx = ast_new(NODE_INDEX, line, col);
            idx->as.index_expr.target = expr;
            idx->as.index_expr.index  = parse_expr(p);
            consume(p, TOK_RBRACKET, "Expected ']'");
            expr = idx;
            continue;
        }
        // Member access:  expr.field
        if (match(p, TOK_DOT)) {
            Token field = consume(p, TOK_IDENT, "Expected field name after '.'");
            ASTNode *mem = ast_new(NODE_MEMBER, line, col);
            mem->as.member.object = expr;
            mem->as.member.field  = tok_str(&field);
            expr = mem;
            continue;
        }
        break;
    }
    return expr;
}

static ASTNode *parse_unary(Parser *p) {
    int line = p->current.line, col = p->current.col;
    if (match(p, TOK_BANG) || match(p, TOK_MINUS)) {
        char *op = tok_str(&p->previous);
        ASTNode *n = ast_new(NODE_UNARY, line, col);
        n->as.unary.op      = op;
        n->as.unary.operand = parse_unary(p);
        return n;
    }
    return parse_postfix(p);
}

static int binary_prec(TokenType t) {
    switch (t) {
        case TOK_OR:    return 1;
        case TOK_AND:   return 2;
        case TOK_EQ: case TOK_NEQ:                          return 3;
        case TOK_LT: case TOK_GT: case TOK_LTE: case TOK_GTE: return 4;
        case TOK_PLUS: case TOK_MINUS:                      return 5;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT:    return 6;
        default: return -1;
    }
}

static ASTNode *parse_binary(Parser *p, int min_prec) {
    ASTNode *left = parse_unary(p);
    while (1) {
        int prec = binary_prec(p->current.type);
        if (prec < min_prec) break;
        int line = p->current.line, col = p->current.col;
        advance(p);
        char *op = tok_str(&p->previous);
        ASTNode *right = parse_binary(p, prec + 1);
        ASTNode *n = ast_new(NODE_BINARY, line, col);
        n->as.binary.op    = op;
        n->as.binary.left  = left;
        n->as.binary.right = right;
        left = n;
    }
    return left;
}

// Assignment:  target = / += / -= / *= /= value
static ASTNode *parse_expr(Parser *p) {
    ASTNode *left = parse_binary(p, 0);
    int line = p->current.line, col = p->current.col;

    TokenType t = p->current.type;
    if (t == TOK_ASSIGN || t == TOK_PLUS_ASSIGN || t == TOK_MINUS_ASSIGN ||
        t == TOK_STAR_ASSIGN || t == TOK_SLASH_ASSIGN) {
        advance(p);
        char *op = tok_str(&p->previous);
        ASTNode *value = parse_expr(p);
        ASTNode *n = ast_new(NODE_ASSIGN, line, col);
        n->as.assign.op     = op;
        n->as.assign.target = left;
        n->as.assign.value  = value;
        return n;
    }
    return left;
}

// ─────────────────────────────────────────────
//  Statement parsing
// ─────────────────────────────────────────────

static ASTNode *parse_block(Parser *p) {
    int line = p->previous.line, col = p->previous.col;
    ASTNode *block = ast_new(NODE_BLOCK, line, col);
    ASTNode **stmts = NULL;
    int count = 0;

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        ASTNode *s = parse_stmt(p);
        if (s) {
            stmts = realloc(stmts, sizeof(ASTNode*) * (count + 1));
            stmts[count++] = s;
        }
    }
    consume(p, TOK_RBRACE, "Expected '}' to close block");
    block->as.block.stmts = stmts;
    block->as.block.count = count;
    return block;
}

// fn name(params) -> RetType { body }
static ASTNode *parse_fn_def(Parser *p, int is_unsafe) {
    int line = p->previous.line, col = p->previous.col;
    Token name_tok = consume(p, TOK_IDENT, "Expected function name");

    ASTNode *fn = ast_new(NODE_FN_DEF, line, col);
    fn->as.fn_def.name      = tok_str(&name_tok);
    fn->as.fn_def.is_unsafe = is_unsafe;

    consume(p, TOK_LPAREN, "Expected '(' after function name");

    Param *params = NULL;
    int    param_count = 0;
    if (!check(p, TOK_RPAREN)) {
        do {
            Token pname = consume(p, TOK_IDENT, "Expected parameter name");
            consume(p, TOK_COLON, "Expected ':' after parameter name");
            ASTNode *ptype = parse_type(p);
            params = realloc(params, sizeof(Param) * (param_count + 1));
            params[param_count].name = tok_str(&pname);
            params[param_count].type = ptype;
            param_count++;
        } while (match(p, TOK_COMMA));
    }
    consume(p, TOK_RPAREN, "Expected ')'");

    fn->as.fn_def.params      = params;
    fn->as.fn_def.param_count = param_count;

    // Optional return type: -> Type
    if (match(p, TOK_ARROW))
        fn->as.fn_def.return_type = parse_type(p);
    else
        fn->as.fn_def.return_type = NULL;

    consume(p, TOK_LBRACE, "Expected '{' before function body");
    fn->as.fn_def.body = parse_block(p);
    return fn;
}

// entity Name { fields and methods }
static ASTNode *parse_entity_def(Parser *p) {
    int line = p->previous.line, col = p->previous.col;
    Token name_tok = consume(p, TOK_IDENT, "Expected entity name");
    consume(p, TOK_LBRACE, "Expected '{'");

    ASTNode *entity = ast_new(NODE_ENTITY_DEF, line, col);
    entity->as.entity_def.name = tok_str(&name_tok);

    ASTNode **members = NULL;
    int count = 0;

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        ASTNode *member = NULL;

        if (check(p, TOK_FN)) {
            advance(p);
            member = parse_fn_def(p, 0);
        } else if (check(p, TOK_UNSAFE) ) {
            advance(p);
            if (check(p, TOK_FN)) { advance(p); member = parse_fn_def(p, 1); }
        } else if (check(p, TOK_IDENT)) {
            // field:  name: type
            int fline = p->current.line, fcol = p->current.col;
            Token fname = p->current; advance(p);
            consume(p, TOK_COLON, "Expected ':' after field name");
            ASTNode *ftype = parse_type(p);
            member = ast_new(NODE_LET, fline, fcol);
            member->as.let.name      = tok_str(&fname);
            member->as.let.type_node = ftype;
            member->as.let.init      = NULL;
        } else {
            advance(p); // skip unknown
        }

        if (member) {
            members = realloc(members, sizeof(ASTNode*) * (count + 1));
            members[count++] = member;
        }
    }
    consume(p, TOK_RBRACE, "Expected '}' after entity body");
    entity->as.entity_def.members      = members;
    entity->as.entity_def.member_count = count;
    return entity;
}

static ASTNode *parse_stmt(Parser *p) {
    int line = p->current.line, col = p->current.col;

    // use std.io  (segments can be keywords like 'tensor', 'sys', etc.)
    if (match(p, TOK_USE)) {
        ASTNode *n = ast_new(NODE_USE, line, col);
        char path[256] = {0};
        // accept any token as a namespace segment (ident or keyword)
        if (p->current.type == TOK_EOF) {
            error_at(p, &p->current, "Expected namespace name");
            return n;
        }
        strncat(path, p->current.start, p->current.length);
        advance(p);
        while (match(p, TOK_DOT)) {
            strcat(path, ".");
            if (p->current.type == TOK_EOF) break;
            strncat(path, p->current.start, p->current.length);
            advance(p);
        }
        n->as.use.path = strdup(path);
        return n;
    }

    // unsafe { ... } or unsafe fn
    if (match(p, TOK_UNSAFE)) {
        if (check(p, TOK_FN)) { advance(p); return parse_fn_def(p, 1); }
        consume(p, TOK_LBRACE, "Expected '{' after unsafe");
        ASTNode *block = parse_block(p);
        block->type = NODE_UNSAFE_BLOCK;
        return block;
    }

    // fn
    if (match(p, TOK_FN)) return parse_fn_def(p, 0);

    // entity
    if (match(p, TOK_ENTITY)) return parse_entity_def(p);

    // let
    if (match(p, TOK_LET)) {
        ASTNode *n = ast_new(NODE_LET, line, col);
        Token name_tok = consume(p, TOK_IDENT, "Expected variable name");
        n->as.let.name = tok_str(&name_tok);

        if (match(p, TOK_COLON))
            n->as.let.type_node = parse_type(p);
        else
            n->as.let.type_node = NULL;

        if (match(p, TOK_ASSIGN))
            n->as.let.init = parse_expr(p);
        else
            n->as.let.init = NULL;

        return n;
    }

    // return
    if (match(p, TOK_RETURN)) {
        ASTNode *n = ast_new(NODE_RETURN, line, col);
        if (!check(p, TOK_RBRACE) && !check(p, TOK_EOF))
            n->as.ret.value = parse_expr(p);
        return n;
    }

    // try / catch
    if (match(p, TOK_TRY)) {
        ASTNode *n = ast_new(NODE_TRY, line, col);
        consume(p, TOK_LBRACE, "Expected '{' after try");
        n->as.try_stmt.try_block = parse_block(p);
        n->as.try_stmt.catch_var[0] = '\0';
        n->as.try_stmt.catch_block = NULL;
        if (match(p, TOK_CATCH)) {
            // optional: catch (err)
            if (match(p, TOK_LPAREN)) {
                if (check(p, TOK_IDENT)) {
                    Token t = p->current;
                    advance(p);
                    char *name = tok_str(&t);
                    strncpy(n->as.try_stmt.catch_var, name, 63);
                    free(name);
                }
                consume(p, TOK_RPAREN, "Expected ')' after catch variable");
            }
            consume(p, TOK_LBRACE, "Expected '{' after catch");
            n->as.try_stmt.catch_block = parse_block(p);
        }
        return n;
    }
    // throw
    if (match(p, TOK_THROW)) {
        ASTNode *n = ast_new(NODE_THROW, line, col);
        n->as.throw_stmt.value = parse_expr(p);
        return n;
    }
    // if / else if / else
    if (match(p, TOK_IF)) {
        ASTNode *n = ast_new(NODE_IF, line, col);
        n->as.if_stmt.condition  = parse_expr(p);
        consume(p, TOK_LBRACE, "Expected '{' after if condition");
        n->as.if_stmt.then_block = parse_block(p);

        if (match(p, TOK_ELSE)) {
            if (check(p, TOK_IF)) {
                n->as.if_stmt.else_block = parse_stmt(p); // recursive else-if
            } else {
                consume(p, TOK_LBRACE, "Expected '{' after else");
                n->as.if_stmt.else_block = parse_block(p);
            }
        } else {
            n->as.if_stmt.else_block = NULL;
        }
        return n;
    }

    // while
    if (match(p, TOK_WHILE)) {
        ASTNode *n = ast_new(NODE_WHILE, line, col);
        n->as.while_stmt.condition = parse_expr(p);
        consume(p, TOK_LBRACE, "Expected '{' after while condition");
        n->as.while_stmt.body = parse_block(p);
        return n;
    }

    // for i in 0..10 { }  OR  for val in arr { }
    if (match(p, TOK_FOR)) {
        Token var_tok = consume(p, TOK_IDENT, "Expected loop variable");
        consume(p, TOK_IN, "Expected 'in'");

        ASTNode *start_or_iter = parse_expr(p);

        if (match(p, TOK_RANGE)) {
            // for i in 0..10
            ASTNode *n = ast_new(NODE_FOR_RANGE, line, col);
            n->as.for_range.var   = tok_str(&var_tok);
            n->as.for_range.start = start_or_iter;
            n->as.for_range.end   = parse_expr(p);
            consume(p, TOK_LBRACE, "Expected '{'");
            n->as.for_range.body  = parse_block(p);
            return n;
        } else {
            // for val in arr
            ASTNode *n = ast_new(NODE_FOR_ITER, line, col);
            n->as.for_iter.var      = tok_str(&var_tok);
            n->as.for_iter.iterable = start_or_iter;
            consume(p, TOK_LBRACE, "Expected '{'");
            n->as.for_iter.body     = parse_block(p);
            return n;
        }
    }

    // Expression statement (assignment, call, etc.)
    ASTNode *expr = parse_expr(p);
    return expr;
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────
void parser_init(Parser *p, Lexer *lexer) {
    p->lexer      = lexer;
    p->had_error  = 0;
    p->panic_mode = 0;
    // Prime the pump — skip to first real token
    p->current.type = TOK_NEWLINE; // dummy
    advance(p);
}

ASTNode *parser_run(Parser *p) {
    ASTNode *program = ast_new(NODE_PROGRAM, 1, 1);
    ASTNode **stmts  = NULL;
    int count = 0;

    while (!check(p, TOK_EOF)) {
        ASTNode *s = parse_stmt(p);
        if (s) {
            stmts = realloc(stmts, sizeof(ASTNode*) * (count + 1));
            stmts[count++] = s;
        }
        p->panic_mode = 0; // reset after each top-level statement
    }

    program->as.program.stmts = stmts;
    program->as.program.count = count;
    return program;
}