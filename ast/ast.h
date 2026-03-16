#ifndef AST_H
#define AST_H

// ─────────────────────────────────────────────
//  AST Node Types
// ─────────────────────────────────────────────
typedef enum {
    // Statements
    NODE_PROGRAM,
    NODE_USE,
    NODE_FN_DEF,
    NODE_ENTITY_DEF,
    NODE_LET,
    NODE_TRY,
    NODE_THROW,
    NODE_RETURN,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR_RANGE,
    NODE_FOR_ITER,
    NODE_UNSAFE_BLOCK,
    NODE_BLOCK,
    NODE_EXPR_STMT,

    // Expressions
    NODE_ASSIGN,
    NODE_BINARY,
    NODE_UNARY,
    NODE_CALL,
    NODE_INDEX,
    NODE_MEMBER,
    NODE_IDENT,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_CHAR_LIT,
    NODE_BOOL_LIT,
    NODE_ARRAY_LIT,

    // Types
    NODE_TYPE,
    NODE_POINTER_TYPE,
    NODE_ARRAY_TYPE,
} NodeType;

typedef struct ASTNode ASTNode;

// ─────────────────────────────────────────────
//  Parameter
// ─────────────────────────────────────────────
typedef struct {
    char    *name;
    ASTNode *type;
} Param;

// ─────────────────────────────────────────────
//  ASTNode
// ─────────────────────────────────────────────
struct ASTNode {
    NodeType type;
    int      line;
    int      col;

    union {
        struct { ASTNode **stmts;   int count; }            program;
        struct { char *path; }                              use;
        struct {
            char    *name;
            Param   *params;
            int      param_count;
            ASTNode *return_type;
            ASTNode *body;
            int      is_unsafe;
        } fn_def;
        struct {
            char     *name;
            ASTNode **members;
            int       member_count;
        } entity_def;
        struct {
            char    *name;
            ASTNode *type_node;
            ASTNode *init;
        } let;
        struct { ASTNode *value; }                          ret;
        struct {
            ASTNode *condition;
            ASTNode *then_block;
            ASTNode *else_block;
        } if_stmt;
        struct { ASTNode *try_block; char catch_var[64]; ASTNode *catch_block; } try_stmt;
        struct { ASTNode *value; } throw_stmt;
        struct { ASTNode *condition; ASTNode *body; }       while_stmt;
        struct {
            char    *var;
            ASTNode *start;
            ASTNode *end;
            ASTNode *body;
        } for_range;
        struct {
            char    *var;
            ASTNode *iterable;
            ASTNode *body;
        } for_iter;
        struct { ASTNode **stmts; int count; }              block;
        struct {
            char    *op;
            ASTNode *target;
            ASTNode *value;
        } assign;
        struct {
            char    *op;
            ASTNode *left;
            ASTNode *right;
        } binary;
        struct { char *op; ASTNode *operand; }              unary;
        struct {
            ASTNode  *callee;
            ASTNode **args;
            int       arg_count;
        } call;
        struct { ASTNode *target; ASTNode *index; }         index_expr;
        struct { ASTNode *object; char *field; }            member;
        struct { char *name; }                              ident;
        struct { long long value; }                         int_lit;
        struct { double value; }                            float_lit;
        struct { char *value; }                             string_lit;
        struct { char value; }                              char_lit;
        struct { int value; }                               bool_lit;
        struct { ASTNode **elements; int count; }           array_lit;
        struct { char *name; }                              type_node;
        struct { ASTNode *base; }                           pointer_type;
        struct { ASTNode *elem_type; int size; }            array_type;
    } as;
};

// ─────────────────────────────────────────────
//  API
// ─────────────────────────────────────────────
ASTNode *ast_new   (NodeType type, int line, int col);
void     ast_free  (ASTNode *node);
void     ast_print (const ASTNode *node, int indent);

#endif /* AST_H */