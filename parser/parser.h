#ifndef PARSER_H
#define PARSER_H

#include "../lexer/lexer.h"
#include "../ast/ast.h"

// ─────────────────────────────────────────────
//  Parser
// ─────────────────────────────────────────────
typedef struct {
    Lexer   *lexer;
    Token    current;   // current token
    Token    previous;  // last consumed token
    int      had_error;
    int      panic_mode;
} Parser;

// ─────────────────────────────────────────────
//  API
// ─────────────────────────────────────────────
void     parser_init    (Parser *parser, Lexer *lexer);
ASTNode *parser_run     (Parser *parser);   // parse full program → NODE_PROGRAM

#endif /* PARSER_H */