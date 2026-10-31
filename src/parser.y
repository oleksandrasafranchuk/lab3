%code requires {
#include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

int yylex(void);
extern int yylineno;
extern char *yytext;
void yyerror(const char *s);

Node *root;
int parse_errors;

static Node *op2(Kind k, const char *op, Node *a, Node *b, int line)
{
    Node *n = mk(k, line, 2, a, b);
    n->op = strdup(op);
    return n;
}

static Node *op1(Kind k, const char *op, Node *a, int line)
{
    Node *n = mk(k, line, 1, a);
    n->op = strdup(op);
    return n;
}
%}

%locations
%define parse.error verbose
%define parse.trace

%union {
    long long ival;
    char *str;
    Node *node;
}

%token <ival> INUM
%token <str>  FNUM CHARLIT STRLIT IDENT
%token KW_INT KW_CHAR KW_FLOAT KW_DOUBLE KW_VOID KW_SHORT KW_LONG KW_UNSIGNED KW_SIGNED KW_CONST
%token KW_IF KW_ELSE KW_WHILE KW_DO KW_FOR KW_RETURN KW_BREAK KW_CONTINUE
%token INC DEC AND OR EQ NE LE GE SHL SHR
%token ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN

%type <node> ext_list ext_def func_def params param_list param decl init_decl_list init_decl
%type <node> declarator initializer init_list block item_list item stmt opt_expr expr args arg_list
%type <str>  type_spec base_type

%nonassoc LOWER_THAN_ELSE
%nonassoc KW_ELSE

%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN
%right '?' ':'
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left '<' '>' LE GE
%left SHL SHR
%left '+' '-'
%left '*' '/' '%'
%right UNARY '!' '~'
%left INC DEC '['

%start program

%%

program
    : ext_list                                  { root = $1; }
    ;

ext_list
    : %empty                                    { $$ = node(N_PROGRAM, 1); }
    | ext_list ext_def                          { $$ = node_add($1, $2); }
    ;

ext_def
    : func_def
    | decl
    ;

func_def
    : type_spec IDENT '(' params ')' block      { $$ = mk(N_FUNC, @2.first_line, 2, $4, $6); $$->type = $1; $$->name = $2; }
    | type_spec IDENT '(' params ')' ';'        { $$ = mk(N_PROTO, @2.first_line, 1, $4); $$->type = $1; $$->name = $2; }
    ;

params
    : %empty                                    { $$ = node(N_PARAMS, yylineno); }
    | KW_VOID                                   { $$ = node(N_PARAMS, @1.first_line); }
    | param_list
    ;

param_list
    : param                                     { $$ = mk(N_PARAMS, @1.first_line, 1, $1); }
    | param_list ',' param                      { $$ = node_add($1, $3); }
    ;

param
    : type_spec IDENT                           { $$ = node(N_PARAM, @2.first_line); $$->type = $1; $$->name = $2; }
    | type_spec IDENT '[' ']'                   { $$ = node(N_PARAM, @2.first_line); $$->type = $1; $$->name = $2; $$->arr = -1; }
    | type_spec IDENT '[' INUM ']'              { $$ = node(N_PARAM, @2.first_line); $$->type = $1; $$->name = $2; $$->arr = -1; }
    ;

type_spec
    : base_type
    | KW_CONST base_type                        { $$ = $2; }
    ;

base_type
    : KW_INT                                    { $$ = "int"; }
    | KW_CHAR                                   { $$ = "char"; }
    | KW_FLOAT                                  { $$ = "double"; }
    | KW_DOUBLE                                 { $$ = "double"; }
    | KW_VOID                                   { $$ = "void"; }
    | KW_SHORT                                  { $$ = "int"; }
    | KW_LONG                                   { $$ = "int"; }
    | KW_LONG KW_INT                            { $$ = "int"; }
    | KW_LONG KW_LONG                           { $$ = "int"; }
    | KW_LONG KW_DOUBLE                         { $$ = "double"; }
    | KW_UNSIGNED                               { $$ = "int"; }
    | KW_UNSIGNED KW_INT                        { $$ = "int"; }
    | KW_UNSIGNED KW_LONG                       { $$ = "int"; }
    | KW_UNSIGNED KW_CHAR                       { $$ = "char"; }
    | KW_SIGNED                                 { $$ = "int"; }
    | KW_SIGNED KW_INT                          { $$ = "int"; }
    ;

decl
    : type_spec init_decl_list ';'
        {
            $$ = $2;
            $$->type = $1;
            for (int i = 0; i < $$->n; i++) $$->kid[i]->type = $1;
        }
    ;

init_decl_list
    : init_decl                                 { $$ = mk(N_DECL, @1.first_line, 1, $1); }
    | init_decl_list ',' init_decl              { $$ = node_add($1, $3); }
    ;

init_decl
    : declarator
    | declarator '=' initializer                { $$ = node_add($1, $3); }
    ;

declarator
    : IDENT                                     { $$ = node(N_VAR, @1.first_line); $$->name = $1; }
    | IDENT '[' INUM ']'                        { $$ = node(N_VAR, @1.first_line); $$->name = $1; $$->arr = (int)$3; }
    | IDENT '[' ']'                             { $$ = node(N_VAR, @1.first_line); $$->name = $1; $$->arr = -1; }
    ;

initializer
    : expr
    | '{' init_list '}'                         { $$ = $2; }
    | '{' init_list ',' '}'                     { $$ = $2; }
    ;

init_list
    : expr                                      { $$ = mk(N_INITLIST, @1.first_line, 1, $1); }
    | init_list ',' expr                        { $$ = node_add($1, $3); }
    ;

block
    : '{' item_list '}'                         { $$ = $2; $$->line = @1.first_line; }
    ;

item_list
    : %empty                                    { $$ = node(N_BLOCK, yylineno); }
    | item_list item                            { $$ = node_add($1, $2); }
    ;

item
    : decl
    | stmt
    ;

stmt
    : block
    | expr ';'                                  { $$ = mk(N_EXPR, @1.first_line, 1, $1); }
    | ';'                                       { $$ = node(N_EMPTY, @1.first_line); }
    | KW_IF '(' expr ')' stmt %prec LOWER_THAN_ELSE
                                                { $$ = mk(N_IF, @1.first_line, 3, $3, $5, NULL); }
    | KW_IF '(' expr ')' stmt KW_ELSE stmt      { $$ = mk(N_IF, @1.first_line, 3, $3, $5, $7); }
    | KW_WHILE '(' expr ')' stmt                { $$ = mk(N_WHILE, @1.first_line, 2, $3, $5); }
    | KW_DO stmt KW_WHILE '(' expr ')' ';'      { $$ = mk(N_DOWHILE, @1.first_line, 2, $2, $5); }
    | KW_FOR '(' opt_expr ';' opt_expr ';' opt_expr ')' stmt
                                                { $$ = mk(N_FOR, @1.first_line, 4, $3, $5, $7, $9); }
    | KW_FOR '(' decl opt_expr ';' opt_expr ')' stmt
                                                { $$ = mk(N_FOR, @1.first_line, 4, $3, $4, $6, $8); }
    | KW_RETURN opt_expr ';'                    { $$ = mk(N_RETURN, @1.first_line, 1, $2); }
    | KW_BREAK ';'                              { $$ = node(N_BREAK, @1.first_line); }
    | KW_CONTINUE ';'                           { $$ = node(N_CONTINUE, @1.first_line); }
    | error ';'                                 { yyerrok; $$ = node(N_EMPTY, @1.first_line); }
    ;

opt_expr
    : %empty                                    { $$ = NULL; }
    | expr
    ;

expr
    : INUM                                      { $$ = node(N_INT, @1.first_line); $$->ival = $1; }
    | FNUM                                      { $$ = node(N_DBL, @1.first_line); $$->name = $1; $$->dval = atof($1); }
    | CHARLIT                                   { $$ = node(N_CHR, @1.first_line); $$->name = $1; $$->ival = char_value($1); }
    | STRLIT                                    { $$ = node(N_STR, @1.first_line); $$->name = $1; $$->sval = unescape($1); }
    | IDENT                                     { $$ = node(N_ID, @1.first_line); $$->name = $1; }
    | '(' expr ')'                              { $$ = $2; }
    | IDENT '(' args ')'                        { $$ = $3; $$->name = $1; $$->line = @1.first_line; }
    | expr '[' expr ']'                         { $$ = mk(N_INDEX, @1.first_line, 2, $1, $3); }
    | expr INC                                  { $$ = op1(N_POSTINC, "++", $1, @2.first_line); }
    | expr DEC                                  { $$ = op1(N_POSTINC, "--", $1, @2.first_line); }
    | INC expr %prec UNARY                      { $$ = op1(N_PREINC, "++", $2, @1.first_line); }
    | DEC expr %prec UNARY                      { $$ = op1(N_PREINC, "--", $2, @1.first_line); }
    | '-' expr %prec UNARY                      { $$ = op1(N_UNOP, "-", $2, @1.first_line); }
    | '+' expr %prec UNARY                      { $$ = op1(N_UNOP, "+", $2, @1.first_line); }
    | '!' expr                                  { $$ = op1(N_UNOP, "!", $2, @1.first_line); }
    | '~' expr                                  { $$ = op1(N_UNOP, "~", $2, @1.first_line); }
    | '(' type_spec ')' expr %prec UNARY        { $$ = mk(N_CAST, @1.first_line, 1, $4); $$->type = $2; }
    | expr '*' expr                             { $$ = op2(N_BINOP, "*",  $1, $3, @2.first_line); }
    | expr '/' expr                             { $$ = op2(N_BINOP, "/",  $1, $3, @2.first_line); }
    | expr '%' expr                             { $$ = op2(N_BINOP, "%",  $1, $3, @2.first_line); }
    | expr '+' expr                             { $$ = op2(N_BINOP, "+",  $1, $3, @2.first_line); }
    | expr '-' expr                             { $$ = op2(N_BINOP, "-",  $1, $3, @2.first_line); }
    | expr SHL expr                             { $$ = op2(N_BINOP, "<<", $1, $3, @2.first_line); }
    | expr SHR expr                             { $$ = op2(N_BINOP, ">>", $1, $3, @2.first_line); }
    | expr '<' expr                             { $$ = op2(N_BINOP, "<",  $1, $3, @2.first_line); }
    | expr '>' expr                             { $$ = op2(N_BINOP, ">",  $1, $3, @2.first_line); }
    | expr LE expr                              { $$ = op2(N_BINOP, "<=", $1, $3, @2.first_line); }
    | expr GE expr                              { $$ = op2(N_BINOP, ">=", $1, $3, @2.first_line); }
    | expr EQ expr                              { $$ = op2(N_BINOP, "==", $1, $3, @2.first_line); }
    | expr NE expr                              { $$ = op2(N_BINOP, "!=", $1, $3, @2.first_line); }
    | expr '&' expr                             { $$ = op2(N_BINOP, "&",  $1, $3, @2.first_line); }
    | expr '^' expr                             { $$ = op2(N_BINOP, "^",  $1, $3, @2.first_line); }
    | expr '|' expr                             { $$ = op2(N_BINOP, "|",  $1, $3, @2.first_line); }
    | expr AND expr                             { $$ = op2(N_BINOP, "&&", $1, $3, @2.first_line); }
    | expr OR expr                              { $$ = op2(N_BINOP, "||", $1, $3, @2.first_line); }
    | expr '?' expr ':' expr                    { $$ = mk(N_TERNARY, @2.first_line, 3, $1, $3, $5); }
    | expr '=' expr                             { $$ = op2(N_ASSIGN, "=",   $1, $3, @2.first_line); }
    | expr ADD_ASSIGN expr                      { $$ = op2(N_ASSIGN, "+=",  $1, $3, @2.first_line); }
    | expr SUB_ASSIGN expr                      { $$ = op2(N_ASSIGN, "-=",  $1, $3, @2.first_line); }
    | expr MUL_ASSIGN expr                      { $$ = op2(N_ASSIGN, "*=",  $1, $3, @2.first_line); }
    | expr DIV_ASSIGN expr                      { $$ = op2(N_ASSIGN, "/=",  $1, $3, @2.first_line); }
    | expr MOD_ASSIGN expr                      { $$ = op2(N_ASSIGN, "%=",  $1, $3, @2.first_line); }
    | expr AND_ASSIGN expr                      { $$ = op2(N_ASSIGN, "&=",  $1, $3, @2.first_line); }
    | expr OR_ASSIGN expr                       { $$ = op2(N_ASSIGN, "|=",  $1, $3, @2.first_line); }
    | expr XOR_ASSIGN expr                      { $$ = op2(N_ASSIGN, "^=",  $1, $3, @2.first_line); }
    | expr SHL_ASSIGN expr                      { $$ = op2(N_ASSIGN, "<<=", $1, $3, @2.first_line); }
    | expr SHR_ASSIGN expr                      { $$ = op2(N_ASSIGN, ">>=", $1, $3, @2.first_line); }
    ;

args
    : %empty                                    { $$ = node(N_CALL, yylineno); }
    | arg_list
    ;

arg_list
    : expr                                      { $$ = mk(N_CALL, @1.first_line, 1, $1); }
    | arg_list ',' expr                         { $$ = node_add($1, $3); }
    ;

%%

void yyerror(const char *s)
{
    fprintf(stderr, "Syntax error (line %d): %s near '%s'\n", yylineno, s, yytext);
    parse_errors++;
}
