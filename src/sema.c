#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ast.h"

typedef struct Sym {
    const char *name, *type;
    int arr, global;
    struct Sym *next;
} Sym;

typedef struct Scope {
    Sym *syms;
    struct Scope *up;
} Scope;

typedef struct Fn {
    const char *name, *type;
    Node *params;
    int defined;
    struct Fn *next;
} Fn;

static Scope *sc;
static Fn *fns;
static int errors, loops;
static Node *curfn;

static const struct { const char *name, *type; int argc; } builtins[] = {
    { "printf", "int", -1 }, { "puts", "int", 1 }, { "putchar", "int", 1 },
    { "abs", "int", 1 }, { "fabs", "double", 1 }, { "sqrt", "double", 1 }, { "pow", "double", 2 },
};

static void err(int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Semantic error (line %d): ", line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    errors++;
}

static void warn(int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Warning (line %d): ", line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void push(void)
{
    Scope *s = calloc(1, sizeof *s);
    s->up = sc;
    sc = s;
}

static void pop(void)
{
    Scope *s = sc;
    sc = s->up;
    while (s->syms) { Sym *x = s->syms; s->syms = x->next; free(x); }
    free(s);
}

static Sym *lookup(const char *name)
{
    for (Scope *s = sc; s; s = s->up)
        for (Sym *x = s->syms; x; x = x->next)
            if (!strcmp(x->name, name)) return x;
    return NULL;
}

static void declare(int line, const char *name, const char *type, int arr)
{
    for (Sym *x = sc->syms; x; x = x->next)
        if (!strcmp(x->name, name)) { err(line, "redeclaration of '%s'", name); return; }
    Sym *x = calloc(1, sizeof *x);
    x->name = name;
    x->type = type;
    x->arr = arr;
    x->global = sc->up == NULL;
    x->next = sc->syms;
    sc->syms = x;
}

static Fn *findfn(const char *name)
{
    for (Fn *f = fns; f; f = f->next)
        if (!strcmp(f->name, name)) return f;
    return NULL;
}

static int builtin(const char *name)
{
    for (size_t i = 0; i < sizeof builtins / sizeof *builtins; i++)
        if (!strcmp(builtins[i].name, name)) return (int)i;
    return -1;
}

static const char *arrtype(const char *t)
{
    if (!strcmp(t, "double")) return "double[]";
    if (!strcmp(t, "char")) return "char[]";
    return "int[]";
}

static int is_array(const char *t) { return t && strchr(t, '[') != NULL; }
static int is_scalar(const char *t) { return t && (!strcmp(t, "int") || !strcmp(t, "char") || !strcmp(t, "double")); }
static int is_integer(const char *t) { return t && (!strcmp(t, "int") || !strcmp(t, "char")); }

static const char *arith(const char *a, const char *b)
{
    return (!strcmp(a, "double") || !strcmp(b, "double")) ? "double" : "int";
}

static int lvalue(Node *n)
{
    return (n->kind == N_ID && is_scalar(n->ctype)) || n->kind == N_INDEX;
}

static const char *expr(Node *n);

static const char *scalar(Node *n, const char *what)
{
    const char *t = expr(n);
    if (!is_scalar(t)) { err(n->line, "%s must be a scalar value", what); return "int"; }
    return t;
}

static const char *expr(Node *n)
{
    const char *t = "int";
    switch (n->kind) {
    case N_INT: case N_CHR: t = "int"; break;
    case N_DBL: t = "double"; break;
    case N_STR: t = "string"; break;
    case N_ID: {
        Sym *s = lookup(n->name);
        if (!s) { err(n->line, "undeclared identifier '%s'", n->name); break; }
        n->global = s->global;
        t = s->arr ? arrtype(s->type) : s->type;
        break;
    }
    case N_INDEX: {
        const char *a = expr(n->kid[0]);
        const char *i = expr(n->kid[1]);
        if (!is_array(a)) { err(n->line, "subscripted value is not an array"); break; }
        if (!is_integer(i)) err(n->line, "array index must be an integer");
        t = !strcmp(a, "double[]") ? "double" : !strcmp(a, "char[]") ? "char" : "int";
        break;
    }
    case N_CALL: {
        int b = builtin(n->name);
        for (int i = 0; i < n->n; i++) expr(n->kid[i]);
        if (b >= 0) {
            if (builtins[b].argc >= 0 && n->n != builtins[b].argc)
                err(n->line, "'%s' expects %d argument(s), got %d", n->name, builtins[b].argc, n->n);
            if (!strcmp(n->name, "printf") && (n->n == 0 || n->kid[0]->kind != N_STR))
                err(n->line, "first argument of printf must be a string literal");
            t = builtins[b].type;
            break;
        }
        Fn *f = findfn(n->name);
        if (!f) { err(n->line, "call to undeclared function '%s'", n->name); break; }
        if (f->params->n != n->n) {
            err(n->line, "'%s' expects %d argument(s), got %d", n->name, f->params->n, n->n);
        } else {
            for (int i = 0; i < n->n; i++) {
                Node *p = f->params->kid[i];
                const char *at = n->kid[i]->ctype;
                if (p->arr && !is_array(at)) err(n->line, "argument %d of '%s' must be an array", i + 1, n->name);
                if (!p->arr && !is_scalar(at)) err(n->line, "argument %d of '%s' must be a scalar", i + 1, n->name);
            }
        }
        t = f->type;
        break;
    }
    case N_UNOP: {
        const char *o = scalar(n->kid[0], "operand");
        if (n->op[0] == '!') t = "int";
        else if (n->op[0] == '~') { if (!is_integer(o)) err(n->line, "operand of '~' must be an integer"); t = "int"; }
        else t = arith(o, o);
        break;
    }
    case N_PREINC: case N_POSTINC:
        t = scalar(n->kid[0], "operand");
        if (!lvalue(n->kid[0])) err(n->line, "operand of '%s' is not an lvalue", n->op);
        break;
    case N_BINOP: {
        const char *a = scalar(n->kid[0], "operand");
        const char *b = scalar(n->kid[1], "operand");
        const char *o = n->op;
        if (strchr("+-*/", o[0]) && !o[1]) t = arith(a, b);
        else if (!strcmp(o, "%") || !strcmp(o, "<<") || !strcmp(o, ">>") || !strcmp(o, "&") || !strcmp(o, "|") || !strcmp(o, "^")) {
            if (!is_integer(a) || !is_integer(b)) err(n->line, "operands of '%s' must be integers", o);
            t = "int";
        } else t = "int";
        break;
    }
    case N_ASSIGN: {
        const char *a = expr(n->kid[0]);
        const char *b = scalar(n->kid[1], "assigned value");
        if (!lvalue(n->kid[0])) err(n->line, "left side of '%s' is not an lvalue", n->op);
        if (strchr("%&|^<>", n->op[0]) && (!is_integer(a) || !is_integer(b)))
            err(n->line, "operands of '%s' must be integers", n->op);
        t = is_scalar(a) ? a : "int";
        break;
    }
    case N_TERNARY:
        scalar(n->kid[0], "condition");
        t = arith(scalar(n->kid[1], "branch"), scalar(n->kid[2], "branch"));
        break;
    case N_CAST:
        scalar(n->kid[0], "cast operand");
        t = n->type;
        break;
    default:
        break;
    }
    n->ctype = t;
    return t;
}

static void decl(Node *d)
{
    for (int i = 0; i < d->n; i++) {
        Node *v = d->kid[i];
        Node *init = v->n ? v->kid[0] : NULL;
        if (!strcmp(v->type, "void")) err(v->line, "variable '%s' declared void", v->name);
        if (init && init->kind == N_INITLIST) {
            if (!v->arr) err(v->line, "initializer list for non-array '%s'", v->name);
            if (v->arr < 0) v->arr = init->n;
            if (v->arr > 0 && init->n > v->arr) err(v->line, "too many initializers for '%s'", v->name);
            for (int k = 0; k < init->n; k++) scalar(init->kid[k], "initializer");
        } else if (init) {
            if (v->arr) err(v->line, "array '%s' needs an initializer list", v->name);
            else scalar(init, "initializer");
        } else if (v->arr < 0) {
            err(v->line, "array '%s' has no size", v->name);
        }
        declare(v->line, v->name, v->type, v->arr);
        v->ctype = v->arr ? arrtype(v->type) : v->type;
    }
}

static void stmt(Node *n, int scope);

static void loop_body(Node *b)
{
    loops++;
    stmt(b, 1);
    loops--;
}

static void stmt(Node *n, int scope)
{
    if (!n) return;
    switch (n->kind) {
    case N_BLOCK:
        if (scope) push();
        for (int i = 0; i < n->n; i++) stmt(n->kid[i], 1);
        if (scope) pop();
        break;
    case N_DECL: decl(n); break;
    case N_EXPR: expr(n->kid[0]); break;
    case N_IF:
        scalar(n->kid[0], "condition");
        stmt(n->kid[1], 1);
        stmt(n->kid[2], 1);
        break;
    case N_WHILE:
        scalar(n->kid[0], "condition");
        loop_body(n->kid[1]);
        break;
    case N_DOWHILE:
        loop_body(n->kid[0]);
        scalar(n->kid[1], "condition");
        break;
    case N_FOR:
        push();
        if (n->kid[0]) { if (n->kid[0]->kind == N_DECL) decl(n->kid[0]); else expr(n->kid[0]); }
        if (n->kid[1]) scalar(n->kid[1], "condition");
        if (n->kid[2]) expr(n->kid[2]);
        loop_body(n->kid[3]);
        pop();
        break;
    case N_RETURN:
        if (n->kid[0]) {
            const char *t = expr(n->kid[0]);
            if (!strcmp(curfn->type, "void")) err(n->line, "void function '%s' returns a value", curfn->name);
            else if (!is_scalar(t)) err(n->line, "return value must be a scalar");
        } else if (strcmp(curfn->type, "void")) {
            warn(n->line, "'return' without value in non-void function '%s'", curfn->name);
        }
        break;
    case N_BREAK:
        if (!loops) err(n->line, "'break' outside of a loop");
        break;
    case N_CONTINUE:
        if (!loops) err(n->line, "'continue' outside of a loop");
        break;
    default:
        break;
    }
}

static void func(Node *f)
{
    curfn = f;
    push();
    Node *ps = f->kid[0];
    for (int i = 0; i < ps->n; i++) {
        Node *p = ps->kid[i];
        if (!strcmp(p->type, "void")) err(p->line, "parameter '%s' declared void", p->name);
        declare(p->line, p->name, p->type, p->arr);
        p->ctype = p->arr ? arrtype(p->type) : p->type;
    }
    stmt(f->kid[1], 0);
    pop();
    curfn = NULL;
}

static void register_fn(Node *f)
{
    if (builtin(f->name) >= 0) { err(f->line, "'%s' is a built-in function", f->name); return; }
    Fn *e = findfn(f->name);
    if (e) {
        if (e->params->n != f->kid[0]->n || strcmp(e->type, f->type))
            err(f->line, "conflicting declaration of '%s'", f->name);
        if (f->kind == N_FUNC) {
            if (e->defined) err(f->line, "redefinition of '%s'", f->name);
            e->defined = 1;
            e->params = f->kid[0];
        }
        return;
    }
    e = calloc(1, sizeof *e);
    e->name = f->name;
    e->type = f->type;
    e->params = f->kid[0];
    e->defined = f->kind == N_FUNC;
    e->next = fns;
    fns = e;
}

int sema(Node *prog)
{
    errors = 0;
    push();
    for (int i = 0; i < prog->n; i++) {
        Node *d = prog->kid[i];
        if (d->kind == N_FUNC || d->kind == N_PROTO) register_fn(d);
    }
    for (int i = 0; i < prog->n; i++) {
        Node *d = prog->kid[i];
        if (d->kind == N_FUNC) func(d);
        else if (d->kind == N_DECL) decl(d);
    }
    for (Fn *f = fns; f; f = f->next)
        if (!f->defined) err(f->params->line, "function '%s' declared but never defined", f->name);
    Fn *m = findfn("main");
    if (!m) err(1, "no 'main' function");
    else if (m->params->n) err(m->params->line, "'main' must take no parameters");
    pop();
    return errors;
}
