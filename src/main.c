#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern FILE *yyin;
int yyparse(void);
extern int yydebug;

static void usage(const char *p)
{
    fprintf(stderr,
        "Usage: %s [options] file.c\n"
        "  -t           print AST as a tree\n"
        "  -y           trace the LALR parser (shift/reduce steps)\n"
        "  -r           run the program (interpreter)\n"
        "  -p out.py    translate to Python ('-' = stdout)\n"
        "  -j out.json  write AST as JSON\n"
        "  -v out.js    write AST for the web visualizer (viz/ast.js)\n", p);
    exit(2);
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char *s = malloc(n + 1);
    s[fread(s, 1, n, f)] = 0;
    fclose(f);
    return s;
}

static FILE *open_out(const char *path)
{
    if (!strcmp(path, "-")) return stdout;
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(2); }
    return f;
}

static void close_out(FILE *f)
{
    if (f != stdout) fclose(f);
}

int main(int argc, char **argv)
{
    int tree = 0, run = 0;
    const char *py = NULL, *json = NULL, *viz = NULL, *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t")) tree = 1;
        else if (!strcmp(argv[i], "-r")) run = 1;
        else if (!strcmp(argv[i], "-y")) yydebug = 1;
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) py = argv[++i];
        else if (!strcmp(argv[i], "-j") && i + 1 < argc) json = argv[++i];
        else if (!strcmp(argv[i], "-v") && i + 1 < argc) viz = argv[++i];
        else if (argv[i][0] == '-') usage(argv[0]);
        else file = argv[i];
    }
    if (!file) usage(argv[0]);

    char *source = slurp(file);
    if (!source || !(yyin = fopen(file, "r"))) { perror(file); return 2; }

    yyparse();
    if (parse_errors || !root) {
        fprintf(stderr, "%d syntax error(s), stopping.\n", parse_errors);
        return 1;
    }

    int sem = sema(root);

    if (tree) ast_print(root, stdout);
    if (json) {
        FILE *f = open_out(json);
        ast_json(root, file, source, f);
        fputc('\n', f);
        close_out(f);
    }
    if (viz) {
        FILE *f = open_out(viz);
        fputs("window.AST_DATA = ", f);
        ast_json(root, file, source, f);
        fputs(";\n", f);
        close_out(f);
    }

    if (sem) {
        fprintf(stderr, "%d semantic error(s).\n", sem);
        return 1;
    }

    if (py) {
        FILE *f = open_out(py);
        gen_python(root, f);
        close_out(f);
    }
    if (!tree && !json && !viz && !py && !run)
        printf("OK: %s parsed, %d AST nodes, no errors.\n", file, ast_count(root));
    if (run) return interpret(root);
    return 0;
}
