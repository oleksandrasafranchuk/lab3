%option noyywrap yylineno nounput noinput

%{
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "parser.tab.h"

#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
%}

D   [0-9]
L   [a-zA-Z_]
H   [0-9a-fA-F]
E   [eE][+-]?{D}+

%%

"/*"([^*]|\*+[^*/])*\*+"/"   ;
"//".*                       ;
^[ \t]*#.*                   ;
[ \t\r\n]+                   ;

"int"       return KW_INT;
"char"      return KW_CHAR;
"float"     return KW_FLOAT;
"double"    return KW_DOUBLE;
"void"      return KW_VOID;
"short"     return KW_SHORT;
"long"      return KW_LONG;
"unsigned"  return KW_UNSIGNED;
"signed"    return KW_SIGNED;
"const"     return KW_CONST;
"if"        return KW_IF;
"else"      return KW_ELSE;
"while"     return KW_WHILE;
"do"        return KW_DO;
"for"       return KW_FOR;
"return"    return KW_RETURN;
"break"     return KW_BREAK;
"continue"  return KW_CONTINUE;

{L}({L}|{D})*                   { yylval.str = strdup(yytext); return IDENT; }

{D}+"."{D}*{E}?[fF]?            |
"."{D}+{E}?[fF]?                |
{D}+{E}[fF]?                    { yylval.str = strdup(yytext); return FNUM; }

0[xX]{H}+[uUlL]*                |
{D}+[uUlL]*                     { yylval.ival = strtoll(yytext, NULL, 0); return INUM; }

\'(\\.|[^\\'\n])+\'             { yylval.str = strdup(yytext); return CHARLIT; }
\"(\\.|[^\\"\n])*\"             { yylval.str = strdup(yytext); return STRLIT; }

"+="    return ADD_ASSIGN;
"-="    return SUB_ASSIGN;
"*="    return MUL_ASSIGN;
"/="    return DIV_ASSIGN;
"%="    return MOD_ASSIGN;
"&="    return AND_ASSIGN;
"|="    return OR_ASSIGN;
"^="    return XOR_ASSIGN;
"<<="   return SHL_ASSIGN;
">>="   return SHR_ASSIGN;
"++"    return INC;
"--"    return DEC;
"&&"    return AND;
"||"    return OR;
"=="    return EQ;
"!="    return NE;
"<="    return LE;
">="    return GE;
"<<"    return SHL;
">>"    return SHR;

[-+*/%<>=!~&|^?:;,(){}\[\]]     return yytext[0];

.   { fprintf(stderr, "Lexical error (line %d): unexpected character '%s'\n", yylineno, yytext); parse_errors++; }

%%
