#ifndef AST_H
#define AST_H

#include <stdio.h>

typedef enum {
    N_PROGRAM, N_FUNC, N_PROTO, N_PARAMS, N_PARAM, N_BLOCK, N_DECL, N_VAR, N_INITLIST,
    N_IF, N_WHILE, N_DOWHILE, N_FOR, N_RETURN, N_BREAK, N_CONTINUE, N_EXPR, N_EMPTY,
    N_ASSIGN, N_BINOP, N_UNOP, N_PREINC, N_POSTINC, N_TERNARY, N_CAST, N_CALL, N_INDEX,
    N_ID, N_INT, N_DBL, N_CHR, N_STR
} Kind;

typedef struct Node {
    Kind kind;
    char *op;
    char *name;
    char *type;
    char *sval;
    long long ival;
    double dval;
    int line;
    int arr;
    const char *ctype;
    int global;
    int n, cap;
    struct Node **kid;
} Node;

Node *node(Kind k, int line);
Node *node_add(Node *p, Node *c);
Node *mk(Kind k, int line, int cnt, ...);
const char *kind_name(Kind k);
long long char_value(const char *raw);
char *unescape(const char *raw);
int ast_count(Node *n);
void ast_print(Node *n, FILE *f);
void ast_json(Node *n, FILE *f);

int sema(Node *prog);
int interpret(Node *prog);
void gen_python(Node *prog, FILE *f);

extern Node *root;
extern int parse_errors;

#endif
