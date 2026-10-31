#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "ast.h"

enum { V_VOID, V_INT, V_DBL, V_STR, V_ARR };

typedef struct Array Array;

typedef struct Value {
    int t;
    long long i;
    double d;
    const char *s;
    Array *a;
} Value;

struct Array {
    int n;
    Value *v;
};

typedef struct Var {
    const char *name;
    Value v;
    int owner;
    struct Var *next;
} Var;

typedef struct Env {
    Var *vars;
    struct Env *up;
} Env;

typedef enum { F_NORMAL, F_BREAK, F_CONTINUE, F_RETURN } Flow;

static Env *globals;
static Node *prog;
static Value retval;
static int depth;

static void rt(int line, const char *fmt, ...)
{
    va_list ap;
    fflush(stdout);
    va_start(ap, fmt);
    fprintf(stderr, "Runtime error (line %d): ", line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static Value mkint(long long i) { Value v = { V_INT, i, 0, NULL, NULL }; return v; }
static Value mkdbl(double d) { Value v = { V_DBL, 0, d, NULL, NULL }; return v; }
static Value mkvoid(void) { Value v = { V_VOID, 0, 0, NULL, NULL }; return v; }

static double num(Value v) { return v.t == V_DBL ? v.d : (double)v.i; }
static long long inum(Value v) { return v.t == V_DBL ? (long long)v.d : v.i; }
static int truthy(Value v) { return v.t == V_DBL ? v.d != 0 : v.i != 0; }

static Value conv(const char *type, Value v)
{
    if (!strcmp(type, "double")) return mkdbl(num(v));
    if (!strcmp(type, "char")) return mkint((char)inum(v));
    return mkint(inum(v));
}

static Env *newenv(Env *up)
{
    Env *e = calloc(1, sizeof *e);
    e->up = up;
    return e;
}

static void freeenv(Env *e)
{
    while (e->vars) {
        Var *v = e->vars;
        e->vars = v->next;
        if (v->owner && v->v.a) { free(v->v.a->v); free(v->v.a); }
        free(v);
    }
    free(e);
}

static Var *define(Env *e, const char *name, Value val, int owner)
{
    Var *v = calloc(1, sizeof *v);
    v->name = name;
    v->v = val;
    v->owner = owner;
    v->next = e->vars;
    e->vars = v;
    return v;
}

static Var *lookup(Env *e, const char *name, int line)
{
    for (; e; e = e->up)
        for (Var *v = e->vars; v; v = v->next)
            if (!strcmp(v->name, name)) return v;
    rt(line, "unknown variable '%s'", name);
    return NULL;
}

static Value eval(Node *n, Env *e);
static Flow exec(Node *n, Env *e);

static Value *lval(Node *n, Env *e)
{
    if (n->kind == N_ID) return &lookup(e, n->name, n->line)->v;
    Value a = eval(n->kid[0], e);
    long long i = inum(eval(n->kid[1], e));
    if (i < 0 || i >= a.a->n) rt(n->line, "array index %lld out of bounds [0, %d)", i, a.a->n);
    return &a.a->v[i];
}

static Value store(Value *dst, Value v)
{
    *dst = dst->t == V_DBL ? mkdbl(num(v)) : mkint(inum(v));
    return *dst;
}

static Value binop(const char *o, Value a, Value b, int line)
{
    int d = a.t == V_DBL || b.t == V_DBL;
    if (!strcmp(o, "+")) return d ? mkdbl(num(a) + num(b)) : mkint(a.i + b.i);
    if (!strcmp(o, "-")) return d ? mkdbl(num(a) - num(b)) : mkint(a.i - b.i);
    if (!strcmp(o, "*")) return d ? mkdbl(num(a) * num(b)) : mkint(a.i * b.i);
    if (!strcmp(o, "/")) {
        if (d) return mkdbl(num(a) / num(b));
        if (b.i == 0) rt(line, "integer division by zero");
        return mkint(a.i / b.i);
    }
    if (!strcmp(o, "%")) {
        if (b.i == 0) rt(line, "integer division by zero");
        return mkint(a.i % b.i);
    }
    if (!strcmp(o, "<<")) return mkint(a.i << b.i);
    if (!strcmp(o, ">>")) return mkint(a.i >> b.i);
    if (!strcmp(o, "&"))  return mkint(a.i & b.i);
    if (!strcmp(o, "|"))  return mkint(a.i | b.i);
    if (!strcmp(o, "^"))  return mkint(a.i ^ b.i);
    if (!strcmp(o, "<"))  return mkint(d ? num(a) <  num(b) : a.i <  b.i);
    if (!strcmp(o, ">"))  return mkint(d ? num(a) >  num(b) : a.i >  b.i);
    if (!strcmp(o, "<=")) return mkint(d ? num(a) <= num(b) : a.i <= b.i);
    if (!strcmp(o, ">=")) return mkint(d ? num(a) >= num(b) : a.i >= b.i);
    if (!strcmp(o, "==")) return mkint(d ? num(a) == num(b) : a.i == b.i);
    if (!strcmp(o, "!=")) return mkint(d ? num(a) != num(b) : a.i != b.i);
    rt(line, "unknown operator '%s'", o);
    return mkvoid();
}

static int do_printf(Value *a, int n, int line)
{
    const char *f = a[0].s;
    int ai = 1, cnt = 0;
    char spec[64];
    for (; *f; f++) {
        if (*f != '%') { putchar(*f); cnt++; continue; }
        if (f[1] == '%') { putchar('%'); cnt++; f++; continue; }
        int k = 0;
        spec[k++] = '%';
        f++;
        while (*f && strchr("-+ #0123456789.", *f) && k < 40) spec[k++] = *f++;
        while (*f && strchr("hlLqjzt", *f)) f++;
        char c = *f;
        if (!c) break;
        if (ai >= n) rt(line, "printf: not enough arguments");
        Value v = a[ai++];
        if (c == 'c') {
            spec[k++] = 'c'; spec[k] = 0;
            cnt += printf(spec, (int)inum(v));
        } else if (strchr("diouxX", c)) {
            spec[k++] = 'l'; spec[k++] = 'l'; spec[k++] = c; spec[k] = 0;
            cnt += printf(spec, inum(v));
        } else if (strchr("fFeEgGaA", c)) {
            spec[k++] = c; spec[k] = 0;
            cnt += printf(spec, num(v));
        } else if (c == 's') {
            if (v.t != V_STR) rt(line, "printf: %%s expects a string");
            spec[k++] = 's'; spec[k] = 0;
            cnt += printf(spec, v.s);
        } else {
            rt(line, "printf: unsupported conversion '%%%c'", c);
        }
    }
    return cnt;
}

static Node *findfn(const char *name)
{
    for (int i = 0; i < prog->n; i++)
        if (prog->kid[i]->kind == N_FUNC && !strcmp(prog->kid[i]->name, name)) return prog->kid[i];
    return NULL;
}

static Value call(Node *n, Env *e)
{
    Value args[64];
    if (n->n > 64) rt(n->line, "too many arguments");
    for (int i = 0; i < n->n; i++) args[i] = eval(n->kid[i], e);
    const char *nm = n->name;

    if (!strcmp(nm, "printf"))  return mkint(do_printf(args, n->n, n->line));
    if (!strcmp(nm, "puts"))    { puts(args[0].s ? args[0].s : ""); return mkint(0); }
    if (!strcmp(nm, "putchar")) { putchar((int)inum(args[0])); return mkint(inum(args[0])); }
    if (!strcmp(nm, "abs"))     return mkint(llabs(inum(args[0])));
    if (!strcmp(nm, "fabs"))    return mkdbl(fabs(num(args[0])));
    if (!strcmp(nm, "sqrt"))    return mkdbl(sqrt(num(args[0])));
    if (!strcmp(nm, "pow"))     return mkdbl(pow(num(args[0]), num(args[1])));

    Node *f = findfn(nm);
    if (!f) rt(n->line, "undefined function '%s'", nm);
    if (++depth > 20000) rt(n->line, "stack overflow (recursion too deep)");

    Env *fe = newenv(globals);
    Node *ps = f->kid[0];
    for (int i = 0; i < ps->n; i++) {
        Node *p = ps->kid[i];
        define(fe, p->name, p->arr ? args[i] : conv(p->type, args[i]), 0);
    }
    Node *body = f->kid[1];
    Flow fl = F_NORMAL;
    for (int i = 0; i < body->n && fl == F_NORMAL; i++) fl = exec(body->kid[i], fe);
    freeenv(fe);
    depth--;

    if (!strcmp(f->type, "void")) return mkvoid();
    if (fl == F_RETURN && retval.t != V_VOID) return conv(f->type, retval);
    return conv(f->type, mkint(0));
}

static Value eval(Node *n, Env *e)
{
    switch (n->kind) {
    case N_INT: case N_CHR: return mkint(n->ival);
    case N_DBL: return mkdbl(n->dval);
    case N_STR: { Value v = mkvoid(); v.t = V_STR; v.s = n->sval; return v; }
    case N_ID: return lookup(e, n->name, n->line)->v;
    case N_INDEX: return *lval(n, e);
    case N_CALL: return call(n, e);
    case N_ASSIGN: {
        Value *dst = lval(n->kid[0], e);
        Value r = eval(n->kid[1], e);
        if (strcmp(n->op, "=")) {
            char base[4] = { 0 };
            memcpy(base, n->op, strlen(n->op) - 1);
            r = binop(base, *dst, r, n->line);
        }
        return store(dst, r);
    }
    case N_PREINC: case N_POSTINC: {
        Value *dst = lval(n->kid[0], e);
        Value old = *dst;
        int delta = n->op[0] == '+' ? 1 : -1;
        *dst = dst->t == V_DBL ? mkdbl(dst->d + delta) : mkint(dst->i + delta);
        return n->kind == N_PREINC ? *dst : old;
    }
    case N_UNOP: {
        Value v = eval(n->kid[0], e);
        switch (n->op[0]) {
        case '-': return v.t == V_DBL ? mkdbl(-v.d) : mkint(-v.i);
        case '+': return v;
        case '!': return mkint(!truthy(v));
        case '~': return mkint(~inum(v));
        }
        break;
    }
    case N_BINOP:
        if (!strcmp(n->op, "&&")) return mkint(truthy(eval(n->kid[0], e)) && truthy(eval(n->kid[1], e)));
        if (!strcmp(n->op, "||")) return mkint(truthy(eval(n->kid[0], e)) || truthy(eval(n->kid[1], e)));
        return binop(n->op, eval(n->kid[0], e), eval(n->kid[1], e), n->line);
    case N_TERNARY:
        return conv(n->ctype, eval(truthy(eval(n->kid[0], e)) ? n->kid[1] : n->kid[2], e));
    case N_CAST:
        return conv(n->type, eval(n->kid[0], e));
    default:
        break;
    }
    rt(n->line, "cannot evaluate %s", kind_name(n->kind));
    return mkvoid();
}

static void declvar(Node *v, Env *e)
{
    int dbl = !strcmp(v->type, "double");
    Value zero = dbl ? mkdbl(0) : mkint(0);
    Node *init = v->n ? v->kid[0] : NULL;
    if (v->arr) {
        Array *a = malloc(sizeof *a);
        a->n = v->arr;
        a->v = malloc(sizeof(Value) * a->n);
        for (int i = 0; i < a->n; i++) a->v[i] = zero;
        if (init)
            for (int i = 0; i < init->n; i++) a->v[i] = conv(v->type, eval(init->kid[i], e));
        Value av = mkvoid();
        av.t = V_ARR;
        av.a = a;
        define(e, v->name, av, 1);
    } else {
        define(e, v->name, init ? conv(v->type, eval(init, e)) : zero, 0);
    }
}

static Flow loop_step(Node *body, Env *e, int *stop)
{
    Flow f = exec(body, e);
    *stop = f == F_BREAK || f == F_RETURN;
    return f == F_RETURN ? F_RETURN : F_NORMAL;
}

static Flow exec(Node *n, Env *e)
{
    if (!n) return F_NORMAL;
    int stop = 0;
    Flow f = F_NORMAL;
    switch (n->kind) {
    case N_BLOCK: {
        Env *b = newenv(e);
        for (int i = 0; i < n->n && f == F_NORMAL; i++) f = exec(n->kid[i], b);
        freeenv(b);
        return f;
    }
    case N_DECL:
        for (int i = 0; i < n->n; i++) declvar(n->kid[i], e);
        return F_NORMAL;
    case N_EXPR:
        eval(n->kid[0], e);
        return F_NORMAL;
    case N_IF:
        return truthy(eval(n->kid[0], e)) ? exec(n->kid[1], e) : exec(n->kid[2], e);
    case N_WHILE:
        while (!stop && truthy(eval(n->kid[0], e))) f = loop_step(n->kid[1], e, &stop);
        return f;
    case N_DOWHILE:
        do f = loop_step(n->kid[0], e, &stop); while (!stop && truthy(eval(n->kid[1], e)));
        return f;
    case N_FOR: {
        Env *fe = newenv(e);
        if (n->kid[0]) { if (n->kid[0]->kind == N_DECL) exec(n->kid[0], fe); else eval(n->kid[0], fe); }
        while (!stop && (!n->kid[1] || truthy(eval(n->kid[1], fe)))) {
            f = loop_step(n->kid[3], fe, &stop);
            if (!stop && n->kid[2]) eval(n->kid[2], fe);
        }
        freeenv(fe);
        return f;
    }
    case N_RETURN:
        retval = n->kid[0] ? eval(n->kid[0], e) : mkvoid();
        return F_RETURN;
    case N_BREAK: return F_BREAK;
    case N_CONTINUE: return F_CONTINUE;
    default: return F_NORMAL;
    }
}

int interpret(Node *p)
{
    prog = p;
    globals = newenv(NULL);
    for (int i = 0; i < p->n; i++)
        if (p->kid[i]->kind == N_DECL) exec(p->kid[i], globals);
    Node c = { 0 };
    c.kind = N_CALL;
    c.name = "main";
    c.line = findfn("main")->line;
    Value r = call(&c, globals);
    fflush(stdout);
    return r.t == V_VOID ? 0 : (int)inum(r);
}
