#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// ─────────────────────────────────────────────
//  Token Types
// ─────────────────────────────────────────────
typedef enum {
    // Literals
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STRING_LIT,
    TOK_CHAR_LIT,
    TOK_BOOL_LIT,

    // Identifiers
    TOK_IDENT,

    // Keywords
    TOK_FN,
    TOK_LET,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_WHILE,
    TOK_IN,
    TOK_RETURN,
    TOK_TRY,
    TOK_CATCH,
    TOK_THROW,
    TOK_RESULT,
    TOK_UNSAFE,
    TOK_USE,
    TOK_ENTITY,
    TOK_SELF,
    TOK_ANY,

    // Primitive Types
    TOK_I8, TOK_I16, TOK_I32, TOK_I64,
    TOK_U8, TOK_U16, TOK_U32, TOK_U64,
    TOK_F32, TOK_F64,
    TOK_BOOL_TYPE,
    TOK_STR_TYPE,
    TOK_CHAR_TYPE,
    TOK_TENSOR_TYPE,

    // Arithmetic
    TOK_PLUS,           // +
    TOK_MINUS,          // -
    TOK_STAR,           // * (multiply or pointer)
    TOK_SLASH,          // /
    TOK_PERCENT,        // %

    // Assignment
    TOK_ASSIGN,         // =
    TOK_PLUS_ASSIGN,    // +=
    TOK_MINUS_ASSIGN,   // -=
    TOK_STAR_ASSIGN,    // *=
    TOK_SLASH_ASSIGN,   // /=

    // Comparison
    TOK_EQ,             // ==
    TOK_NEQ,            // !=
    TOK_LT,             // <
    TOK_GT,             // >
    TOK_LTE,            // <=
    TOK_GTE,            // >=

    // Logical / Bitwise
    TOK_AND,            // &
    TOK_OR,             // |
    TOK_BANG,           // !

    // Special
    TOK_ARROW,          // ->
    TOK_RANGE,          // ..
    TOK_DOT,            // .

    // Delimiters
    TOK_LBRACE,         // {
    TOK_RBRACE,         // }
    TOK_LPAREN,         // (
    TOK_RPAREN,         // )
    TOK_LBRACKET,       // [
    TOK_RBRACKET,       // ]
    TOK_COMMA,          // ,
    TOK_COLON,          // :
    TOK_SEMICOLON,      // ;
    TOK_NEWLINE,        // \n  (for semicolon inference)

    // Comments
    TOK_COMMENT,

    // End / Error
    TOK_EOF,
    TOK_ERROR
} TokenType;

// ─────────────────────────────────────────────
//  Token
// ─────────────────────────────────────────────
typedef struct {
    TokenType   type;
    const char *start;   // pointer into source
    int         length;  // byte length
    int         line;    // 1-based
    int         col;     // 1-based

    union {
        long long  int_val;
        double     float_val;
        char       char_val;
        int        bool_val;   // 1=true, 0=false
    } value;
} Token;

// ─────────────────────────────────────────────
//  Lexer
// ─────────────────────────────────────────────
typedef struct {
    const char *source;
    int         pos;
    int         line;
    int         col;
    int         length;
} Lexer;

// ─────────────────────────────────────────────
//  API
// ─────────────────────────────────────────────
void        lexer_init       (Lexer *lexer, const char *source);
Token       lexer_next_token (Lexer *lexer);
const char *token_type_name  (TokenType type);
void        token_print      (const Token *tok);

#endif /* LEXER_H */