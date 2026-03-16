#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ─────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────

static char current(const Lexer *l) {
    if (l->pos >= l->length) return '\0';
    return l->source[l->pos];
}

static char peek(const Lexer *l, int offset) {
    int idx = l->pos + offset;
    if (idx >= l->length) return '\0';
    return l->source[idx];
}

static char advance(Lexer *l) {
    char c = l->source[l->pos++];
    if (c == '\n') { l->line++; l->col = 1; }
    else           { l->col++; }
    return c;
}

static void skip_whitespace_no_newline(Lexer *l) {
    while (l->pos < l->length) {
        char c = current(l);
        if (c == ' ' || c == '\t' || c == '\r')
            advance(l);
        else
            break;
    }
}

static Token make_token(TokenType type, const char *start, int length,
                        int line, int col) {
    Token t;
    memset(&t, 0, sizeof(t));
    t.type   = type;
    t.start  = start;
    t.length = length;
    t.line   = line;
    t.col    = col;
    return t;
}

static Token error_token(const char *start, int line, int col) {
    return make_token(TOK_ERROR, start, 1, line, col);
}

// ─────────────────────────────────────────────
//  Keyword table
// ─────────────────────────────────────────────
typedef struct { const char *word; TokenType type; } Keyword;

static const Keyword keywords[] = {
    {"fn",      TOK_FN},
    {"let",     TOK_LET},
    {"if",      TOK_IF},
    {"else",    TOK_ELSE},
    {"for",     TOK_FOR},
    {"while",   TOK_WHILE},
    {"in",      TOK_IN},
    {"return",  TOK_RETURN},
    {"try",     TOK_TRY},
    {"catch",   TOK_CATCH},
    {"throw",   TOK_THROW},
    {"result",  TOK_RESULT},
    {"unsafe",  TOK_UNSAFE},
    {"use",     TOK_USE},
    {"entity",  TOK_ENTITY},
    {"self",    TOK_SELF},
    {"any",     TOK_ANY},
    {"true",    TOK_BOOL_LIT},
    {"false",   TOK_BOOL_LIT},
    // Primitive types
    {"i8",      TOK_I8},
    {"i16",     TOK_I16},
    {"i32",     TOK_I32},
    {"i64",     TOK_I64},
    {"u8",      TOK_U8},
    {"u16",     TOK_U16},
    {"u32",     TOK_U32},
    {"u64",     TOK_U64},
    {"f32",     TOK_F32},
    {"f64",     TOK_F64},
    {"bool",    TOK_BOOL_TYPE},
    {"str",     TOK_STR_TYPE},
    {"char",    TOK_CHAR_TYPE},
    {"tensor",  TOK_TENSOR_TYPE},
    {NULL,      TOK_ERROR}
};

static TokenType lookup_keyword(const char *start, int len) {
    for (int i = 0; keywords[i].word != NULL; i++) {
        if ((int)strlen(keywords[i].word) == len &&
            strncmp(keywords[i].word, start, len) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

// ─────────────────────────────────────────────
//  Lexer: scanners
// ─────────────────────────────────────────────

static Token scan_line_comment(Lexer *l) {
    const char *start = l->source + l->pos - 2; // include //
    int line = l->line, col = l->col - 2;
    while (l->pos < l->length && current(l) != '\n')
        advance(l);
    int len = (int)((l->source + l->pos) - start);
    return make_token(TOK_COMMENT, start, len, line, col);
}

static Token scan_block_comment(Lexer *l) {
    const char *start = l->source + l->pos - 2;
    int line = l->line, col = l->col - 2;
    while (l->pos < l->length) {
        if (current(l) == '*' && peek(l, 1) == '/') {
            advance(l); advance(l);
            break;
        }
        advance(l);
    }
    int len = (int)((l->source + l->pos) - start);
    return make_token(TOK_COMMENT, start, len, line, col);
}

static Token scan_string(Lexer *l) {
    int line = l->line, col = l->col - 1;
    const char *start = l->source + l->pos - 1; // include opening "
    while (l->pos < l->length && current(l) != '"') {
        if (current(l) == '\\') advance(l); // skip escape char
        advance(l);
    }
    if (current(l) == '"') advance(l); // consume closing "
    int len = (int)((l->source + l->pos) - start);
    return make_token(TOK_STRING_LIT, start, len, line, col);
}

static Token scan_char(Lexer *l) {
    int line = l->line, col = l->col - 1;
    const char *start = l->source + l->pos - 1;
    char val = '\0';
    if (current(l) == '\\') {
        advance(l);
        char esc = advance(l);
        switch (esc) {
            case 'n':  val = '\n'; break;
            case 't':  val = '\t'; break;
            case 'r':  val = '\r'; break;
            case '\\': val = '\\'; break;
            case '\'': val = '\''; break;
            default:   val = esc;
        }
    } else {
        val = advance(l);
    }
    if (current(l) == '\'') advance(l); // consume closing '
    Token t = make_token(TOK_CHAR_LIT, start,
                         (int)((l->source + l->pos) - start), line, col);
    t.value.char_val = val;
    return t;
}

static Token scan_number(Lexer *l, const char *start, int line, int col) {
    int is_float = 0;

    while (isdigit(current(l))) advance(l);

    // Check for float: digit '.' digit  (but NOT range '..')
    if (current(l) == '.' && peek(l, 1) != '.') {
        char after_dot = peek(l, 1);
        if (isdigit(after_dot)) {
            is_float = 1;
            advance(l); // consume '.'
            while (isdigit(current(l))) advance(l);
        }
    }

    // Optional exponent: e/E [+-] digits
    if (current(l) == 'e' || current(l) == 'E') {
        is_float = 1;
        advance(l);
        if (current(l) == '+' || current(l) == '-') advance(l);
        while (isdigit(current(l))) advance(l);
    }

    int len = (int)((l->source + l->pos) - start);
    Token t = make_token(is_float ? TOK_FLOAT_LIT : TOK_INT_LIT,
                         start, len, line, col);
    if (is_float)
        t.value.float_val = atof(start);
    else
        t.value.int_val = atoll(start);
    return t;
}

static Token scan_ident_or_keyword(Lexer *l, const char *start,
                                   int line, int col) {
    while (isalnum(current(l)) || current(l) == '_') advance(l);
    int len = (int)((l->source + l->pos) - start);
    TokenType type = lookup_keyword(start, len);

    Token t = make_token(type, start, len, line, col);
    if (type == TOK_BOOL_LIT) {
        t.value.bool_val = (start[0] == 't') ? 1 : 0;
    }
    return t;
}

// ─────────────────────────────────────────────
//  Public: lexer_init
// ─────────────────────────────────────────────
void lexer_init(Lexer *l, const char *source) {
    l->source = source;
    l->pos    = 0;
    l->line   = 1;
    l->col    = 1;
    l->length = (int)strlen(source);
}

// ─────────────────────────────────────────────
//  Public: lexer_next_token
// ─────────────────────────────────────────────
Token lexer_next_token(Lexer *l) {
    skip_whitespace_no_newline(l);

    if (l->pos >= l->length)
        return make_token(TOK_EOF, l->source + l->pos, 0, l->line, l->col);

    int   line  = l->line;
    int   col   = l->col;
    char  c     = advance(l);
    const char *start = l->source + l->pos - 1;

    // ── Newline ──────────────────────────────
    if (c == '\n')
        return make_token(TOK_NEWLINE, start, 1, line, col);

    // ── Comments ─────────────────────────────
    if (c == '/') {
        if (current(l) == '/') { advance(l); return scan_line_comment(l); }
        if (current(l) == '*') { advance(l); return scan_block_comment(l); }
        if (current(l) == '=') { advance(l); return make_token(TOK_SLASH_ASSIGN, start, 2, line, col); }
        return make_token(TOK_SLASH, start, 1, line, col);
    }

    // ── String / Char literals ────────────────
    if (c == '"') return scan_string(l);
    if (c == '\'') return scan_char(l);

    // ── Numbers ──────────────────────────────
    if (isdigit(c)) return scan_number(l, start, line, col);

    // ── Identifiers / Keywords ────────────────
    if (isalpha(c) || c == '_')
        return scan_ident_or_keyword(l, start, line, col);

    // ── Operators & Delimiters ────────────────
    switch (c) {
        case '+':
            if (current(l) == '=') { advance(l); return make_token(TOK_PLUS_ASSIGN,  start, 2, line, col); }
            return make_token(TOK_PLUS,    start, 1, line, col);
        case '-':
            if (current(l) == '>') { advance(l); return make_token(TOK_ARROW,         start, 2, line, col); }
            if (current(l) == '=') { advance(l); return make_token(TOK_MINUS_ASSIGN,  start, 2, line, col); }
            return make_token(TOK_MINUS,   start, 1, line, col);
        case '*':
            if (current(l) == '=') { advance(l); return make_token(TOK_STAR_ASSIGN,   start, 2, line, col); }
            return make_token(TOK_STAR,    start, 1, line, col);
        case '%': return make_token(TOK_PERCENT,   start, 1, line, col);
        case '=':
            if (current(l) == '=') { advance(l); return make_token(TOK_EQ,    start, 2, line, col); }
            return make_token(TOK_ASSIGN,  start, 1, line, col);
        case '!':
            if (current(l) == '=') { advance(l); return make_token(TOK_NEQ,   start, 2, line, col); }
            return make_token(TOK_BANG,    start, 1, line, col);
        case '<':
            if (current(l) == '=') { advance(l); return make_token(TOK_LTE,   start, 2, line, col); }
            return make_token(TOK_LT,      start, 1, line, col);
        case '>':
            if (current(l) == '=') { advance(l); return make_token(TOK_GTE,   start, 2, line, col); }
            return make_token(TOK_GT,      start, 1, line, col);
        case '&': return make_token(TOK_AND,       start, 1, line, col);
        case '|': return make_token(TOK_OR,        start, 1, line, col);
        case '.':
            if (current(l) == '.') { advance(l); return make_token(TOK_RANGE, start, 2, line, col); }
            return make_token(TOK_DOT,     start, 1, line, col);
        case '{': return make_token(TOK_LBRACE,    start, 1, line, col);
        case '}': return make_token(TOK_RBRACE,    start, 1, line, col);
        case '(': return make_token(TOK_LPAREN,    start, 1, line, col);
        case ')': return make_token(TOK_RPAREN,    start, 1, line, col);
        case '[': return make_token(TOK_LBRACKET,  start, 1, line, col);
        case ']': return make_token(TOK_RBRACKET,  start, 1, line, col);
        case ',': return make_token(TOK_COMMA,     start, 1, line, col);
        case ':': return make_token(TOK_COLON,     start, 1, line, col);
        case ';': return make_token(TOK_SEMICOLON, start, 1, line, col);
        default:  return error_token(start, line, col);
    }
}

// ─────────────────────────────────────────────
//  Public: token_type_name
// ─────────────────────────────────────────────
const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_INT_LIT:     return "INT_LIT";
        case TOK_FLOAT_LIT:   return "FLOAT_LIT";
        case TOK_STRING_LIT:  return "STRING_LIT";
        case TOK_CHAR_LIT:    return "CHAR_LIT";
        case TOK_BOOL_LIT:    return "BOOL_LIT";
        case TOK_IDENT:       return "IDENT";
        case TOK_FN:          return "fn";
        case TOK_LET:         return "let";
        case TOK_IF:          return "if";
        case TOK_ELSE:        return "else";
        case TOK_FOR:         return "for";
        case TOK_WHILE:       return "while";
        case TOK_IN:          return "in";
        case TOK_RETURN:      return "return";
        case TOK_UNSAFE:      return "unsafe";
        case TOK_USE:         return "use";
        case TOK_ENTITY:      return "entity";
        case TOK_SELF:        return "self";
        case TOK_ANY:         return "any";
        case TOK_I8:          return "i8";
        case TOK_I16:         return "i16";
        case TOK_I32:         return "i32";
        case TOK_I64:         return "i64";
        case TOK_U8:          return "u8";
        case TOK_U16:         return "u16";
        case TOK_U32:         return "u32";
        case TOK_U64:         return "u64";
        case TOK_F32:         return "f32";
        case TOK_F64:         return "f64";
        case TOK_BOOL_TYPE:   return "bool";
        case TOK_STR_TYPE:    return "str";
        case TOK_CHAR_TYPE:   return "char";
        case TOK_TENSOR_TYPE: return "tensor";
        case TOK_PLUS:        return "+";
        case TOK_MINUS:       return "-";
        case TOK_STAR:        return "*";
        case TOK_SLASH:       return "/";
        case TOK_PERCENT:     return "%";
        case TOK_ASSIGN:      return "=";
        case TOK_PLUS_ASSIGN: return "+=";
        case TOK_MINUS_ASSIGN:return "-=";
        case TOK_STAR_ASSIGN: return "*=";
        case TOK_SLASH_ASSIGN:return "/=";
        case TOK_EQ:          return "==";
        case TOK_NEQ:         return "!=";
        case TOK_LT:          return "<";
        case TOK_GT:          return ">";
        case TOK_LTE:         return "<=";
        case TOK_GTE:         return ">=";
        case TOK_AND:         return "&";
        case TOK_OR:          return "|";
        case TOK_BANG:        return "!";
        case TOK_ARROW:       return "->";
        case TOK_RANGE:       return "..";
        case TOK_DOT:         return ".";
        case TOK_LBRACE:      return "{";
        case TOK_RBRACE:      return "}";
        case TOK_LPAREN:      return "(";
        case TOK_RPAREN:      return ")";
        case TOK_LBRACKET:    return "[";
        case TOK_RBRACKET:    return "]";
        case TOK_COMMA:       return ",";
        case TOK_COLON:       return ":";
        case TOK_SEMICOLON:   return ";";
        case TOK_NEWLINE:     return "NEWLINE";
        case TOK_COMMENT:     return "COMMENT";
        case TOK_EOF:         return "EOF";
        case TOK_ERROR:       return "ERROR";
        default:              return "UNKNOWN";
    }
}

// ─────────────────────────────────────────────
//  Public: token_print
// ─────────────────────────────────────────────
void token_print(const Token *tok) {
    printf("[%3d:%-3d] %-16s  `%.*s`",
           tok->line, tok->col,
           token_type_name(tok->type),
           tok->length, tok->start);

    if (tok->type == TOK_INT_LIT)
        printf("  => %lld", tok->value.int_val);
    else if (tok->type == TOK_FLOAT_LIT)
        printf("  => %g",   tok->value.float_val);
    else if (tok->type == TOK_BOOL_LIT)
        printf("  => %s",   tok->value.bool_val ? "true" : "false");
    else if (tok->type == TOK_CHAR_LIT)
        printf("  => '%c'", tok->value.char_val);

    printf("\n");
}