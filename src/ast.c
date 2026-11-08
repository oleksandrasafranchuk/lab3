#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ast.h"

static const char *names[] = {
    "Program", "Function", "Prototype", "Params", "Param", "Block", "Decl", "Var", "InitList",
    "If", "While", "DoWhile", "For", "Return", "Break", "Continue", "ExprStmt", "Empty",
    "Assign", "BinOp", "UnOp", "PreIncDec", "PostIncDec", "Ternary", "Cast", "Call", "Index",
    "Id", "Int", "Double", "Char", "String"
};

const char *kind_name(Kind k) { return names[k]; }

Node *node(Kind k, int line)
{
    Node *n = calloc(1, sizeof *n);
    n->kind = k;
    n->line = line;
    return n;
}

Node *node_add(Node *p, Node *c)
{
    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 4;
        p->kid = realloc(p->kid, p->cap * sizeof *p->kid);
    }
    p->kid[p->n++] = c;
    return p;
}

Node *mk(Kind k, int line, int cnt, ...)
{
    va_list ap;
    Node *n = node(k, line);
    va_start(ap, cnt);
    for (int i = 0; i < cnt; i++) node_add(n, va_arg(ap, Node *));
    va_end(ap);
    return n;
}

static int esc(char c)
{
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '0': return '\0';
    case 'a': return '\a';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'v': return '\v';
    default:  return c;
    }
}

long long char_value(const char *raw)
{
    return raw[1] == '\\' ? esc(raw[2]) : (unsigned char)raw[1];
}

char *unescape(const char *raw)
{
    size_t len = strlen(raw);
    char *s = malloc(len), *d = s;
    for (size_t i = 1; i + 1 < len; i++) {
        if (raw[i] == '\\' && i + 2 < len) *d++ = (char)esc(raw[++i]);
        else *d++ = raw[i];
    }
    *d = 0;
    return s;
}

int ast_count(Node *n)
{
    if (!n) return 0;
    int c = 1;
    for (int i = 0; i < n->n; i++) c += ast_count(n->kid[i]);
    return c;
}

static void label(Node *n, char *b, size_t sz)
{
    switch (n->kind) {
    case N_FUNC: case N_PROTO: snprintf(b, sz, "%s %s()", n->type, n->name); break;
    case N_PARAM: snprintf(b, sz, "%s %s%s", n->type, n->name, n->arr ? "[]" : ""); break;
    case N_DECL: snprintf(b, sz, "%s", n->type); break;
    case N_VAR:
        if (n->arr > 0) snprintf(b, sz, "%s[%d]", n->name, n->arr);
        else if (n->arr < 0) snprintf(b, sz, "%s[]", n->name);
        else snprintf(b, sz, "%s", n->name);
        break;
    case N_ASSIGN: case N_BINOP: case N_UNOP: case N_PREINC: case N_POSTINC:
        snprintf(b, sz, "%s", n->op); break;
    case N_TERNARY: snprintf(b, sz, "?:"); break;
    case N_CAST: snprintf(b, sz, "(%s)", n->type); break;
    case N_CALL: case N_ID: case N_DBL: case N_CHR: case N_STR:
        snprintf(b, sz, "%s", n->name); break;
    case N_INT: snprintf(b, sz, "%lld", n->ival); break;
    case N_INDEX: snprintf(b, sz, "[]"); break;
    default: snprintf(b, sz, "%s", kind_name(n->kind)); break;
    }
}

static const char *role(Kind k, int i)
{
    static const char *ifr[] = { "cond", "then", "else" };
    static const char *forr[] = { "init", "cond", "step", "body" };
    switch (k) {
    case N_FUNC:    return i == 0 ? "params" : "body";
    case N_IF:      return ifr[i];
    case N_TERNARY: return ifr[i];
    case N_WHILE:   return i == 0 ? "cond" : "body";
    case N_DOWHILE: return i == 0 ? "body" : "cond";
    case N_FOR:     return forr[i];
    case N_ASSIGN:  return i == 0 ? "target" : "value";
    case N_BINOP:   return i == 0 ? "left" : "right";
    case N_INDEX:   return i == 0 ? "array" : "index";
    case N_VAR:     return "init";
    case N_CALL:    return "arg";
    default:        return NULL;
    }
}

static void print_rec(Node *n, FILE *f, char *prefix, int last, const char *r)
{
    char buf[256];
    label(n, buf, sizeof buf);
    fprintf(f, "%s%s", prefix, last ? "└── " : "├── ");
    if (r) fprintf(f, "%s: ", r);
    if (n->kind == N_PROGRAM || n->kind == N_BLOCK || n->kind == N_PARAMS || n->kind == N_INITLIST ||
        n->kind == N_EXPR || n->kind == N_BREAK || n->kind == N_CONTINUE || n->kind == N_EMPTY ||
        n->kind == N_RETURN || n->kind == N_IF || n->kind == N_WHILE || n->kind == N_DOWHILE || n->kind == N_FOR)
        fprintf(f, "%s", kind_name(n->kind));
    else
        fprintf(f, "%s %s", kind_name(n->kind), buf);
    if (n->ctype) fprintf(f, "  : %s", n->ctype);
    fprintf(f, "  (line %d)\n", n->line);

    size_t len = strlen(prefix);
    char *np = malloc(len + 8);
    strcpy(np, prefix);
    strcat(np, last ? "    " : "│   ");
    int lastIdx = -1;
    for (int i = 0; i < n->n; i++) if (n->kid[i]) lastIdx = i;
    for (int i = 0; i < n->n; i++)
        if (n->kid[i]) print_rec(n->kid[i], f, np, i == lastIdx, role(n->kind, i));
    free(np);
}

void ast_print(Node *n, FILE *f)
{
    char p[1] = "";
    print_rec(n, f, p, 1, NULL);
}

static void jstr(FILE *f, const char *s)
{
    fputc('"', f);
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') fprintf(f, "\\%c", c);
        else if (c == '\n') fputs("\\n", f);
        else if (c == '\t') fputs("\\t", f);
        else if (c == '\r') fputs("\\r", f);
        else if (c < 0x20) fprintf(f, "\\u%04x", c);
        else fputc(c, f);
    }
    fputc('"', f);
}

static void json_rec(Node *n, FILE *f, const char *r)
{
    char buf[256];
    label(n, buf, sizeof buf);
    fprintf(f, "{\"kind\":");
    jstr(f, kind_name(n->kind));
    fprintf(f, ",\"label\":");
    jstr(f, buf);
    fprintf(f, ",\"line\":%d", n->line);
    if (n->ctype) { fprintf(f, ",\"ctype\":"); jstr(f, n->ctype); }
    if (r) { fprintf(f, ",\"role\":"); jstr(f, r); }
    fprintf(f, ",\"children\":[");
    int first = 1;
    for (int i = 0; i < n->n; i++) {
        if (!n->kid[i]) continue;
        if (!first) fputc(',', f);
        first = 0;
        json_rec(n->kid[i], f, role(n->kind, i));
    }
    fprintf(f, "]}");
}

void ast_json(Node *n, FILE *f)
{
    json_rec(n, f, NULL);
    fputc('\n', f);
}
