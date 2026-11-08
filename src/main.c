#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern FILE *yyin;
int yyparse(void);

int main(int argc, char **argv)
{
    int tree = 0, run = 0, json = 0, py = 0;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t")) tree = 1;
        else if (!strcmp(argv[i], "-r")) run = 1;
        else if (!strcmp(argv[i], "-j")) json = 1;
        else if (!strcmp(argv[i], "-p")) py = 1;
        else file = argv[i];
    }
    if (!file) {
        fprintf(stderr, "Usage: %s [-t] [-r] [-p] [-j] file.c\n", argv[0]);
        return 2;
    }

    yyin = strcmp(file, "-") ? fopen(file, "r") : stdin;
    if (!yyin) {
        perror(file);
        return 2;
    }

    yyparse();
    if (parse_errors || !root) return 1;

    int errors = sema(root);
    if (tree) ast_print(root, stdout);
    if (json) ast_json(root, stdout);
    if (errors) return 1;

    if (py) gen_python(root, stdout);
    if (run) return interpret(root);
    if (!tree && !json && !py) printf("OK\n");
    return 0;
}
