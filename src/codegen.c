#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ast.h"

enum { P_LOW, P_TERN, P_OR, P_AND, P_NOT, P_CMP, P_BOR, P_BXOR, P_BAND, P_SHIFT, P_ADD, P_MUL, P_UNARY, P_POSTFIX, P_ATOM };

typedef struct { char *s; int p; } Ex;

static FILE *out;
static int ind, lines;
static Node *prog, *curfn;

static struct { Kind kind; Node *n; } loops[256];
static int nloops;

static const struct { const char *name, *code; } helpers[] = {
    { "printf",
      "def printf(fmt, *args):\n"
      "    fmt = re.sub(r'%([-+ #0]*\\d*(?:\\.\\d+)?)(?:hh|h|ll|l|L|z|j|t)', r'%\\1', fmt)\n"
      "    s = fmt % args\n"
      "    sys.stdout.write(s)\n"
      "    return len(s)\n" },
    { "puts",
      "def puts(s):\n"
      "    sys.stdout.write(s + '\\n')\n"
      "    return 0\n" },
    { "putchar",
      "def putchar(c):\n"
      "    sys.stdout.write(chr(c))\n"
      "    return c\n" },
    { "_idiv",
      "def _idiv(a, b):\n"
      "    q = abs(a) // abs(b)\n"
      "    return q if (a >= 0) == (b >= 0) else -q\n" },
    { "_imod",
      "def _imod(a, b):\n"
      "    return a - b * _idiv(a, b)\n" },
    { "_set",
      "def _set(a, i, v):\n"
      "    a[i] = v\n"
      "    return v\n" },
    { "_preinc",
      "def _preinc(a, i, d):\n"
      "    a[i] += d\n"
      "    return a[i]\n" },
    { "_postinc",
      "def _postinc(a, i, d):\n"
      "    a[i] += d\n"
      "    return a[i] - d\n" },
};

static int uses(const char *text, const char *name)
{
    size_t len = strlen(name);
    for (const char *p = strstr(text, name); p; p = strstr(p + 1, name)) {
        int before = p > text && (p[-1] == '_' || p[-1] == '.' || (p[-1] >= 'a' && p[-1] <= 'z') || (p[-1] >= 'A' && p[-1] <= 'Z') || (p[-1] >= '0' && p[-1] <= '9'));
        if (!before && p[len] == '(') return 1;
    }
    return 0;
}

static const char *pykw[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await", "class", "def", "del", "elif",
    "except", "finally", "from", "global", "import", "in", "is", "lambda", "nonlocal", "not", "or",
    "pass", "raise", "try", "with", "yield", "int", "float", "bool", "len", "list", "str", "print",
    "math", "re", "sys", "range", "chr", "ord", "type", "id", "input", "min", "max", "sum", NULL
};

static char *fmt(const char *f, ...)
{
    va_list ap;
    char *s;
    va_start(ap, f);
    if (vasprintf(&s, f, ap) < 0) s = NULL;
    va_end(ap);
    return s;
}

static const char *pyname(const char *n)
{
    for (int i = 0; pykw[i]; i++)
        if (!strcmp(pykw[i], n)) return fmt("%s_", n);
    return n;
}

static void emit(const char *f, ...)
{
    va_list ap;
    for (int i = 0; i < ind; i++) fputs("    ", out);
    va_start(ap, f);
    vfprintf(out, f, ap);
    va_end(ap);
    fputc('\n', out);
    lines++;
}

static Ex ex(char *s, int p) { Ex e = { s, p }; return e; }

static char *wrap(Ex e, int need, int strict)
{
    return (e.p < need || (strict && e.p == need)) ? fmt("(%s)", e.s) : e.s;
}

static int is_int(const char *t) { return t && (!strcmp(t, "int") || !strcmp(t, "char")); }
static int is_dbl(const char *t) { return t && !strcmp(t, "double"); }

static int boolish(Node *n)
{
    if (n->kind == N_UNOP && n->op[0] == '!') return 1;
    if (n->kind != N_BINOP) return 0;
    const char *o = n->op;
    return !strcmp(o, "<") || !strcmp(o, ">") || !strcmp(o, "<=") || !strcmp(o, ">=") ||
           !strcmp(o, "==") || !strcmp(o, "!=") || !strcmp(o, "&&") || !strcmp(o, "||");
}

static Ex expr(Node *n, int cond);

static Ex conv(const char *target, Node *src)
{
    Ex e = expr(src, 0);
    if (is_int(target) && (is_dbl(src->ctype) || boolish(src))) return ex(fmt("int(%s)", e.s), P_POSTFIX);
    if (is_dbl(target) && src->kind == N_INT) return ex(fmt("%lld.0", src->ival), P_ATOM);
    return e;
}

static Ex arith(const char *o, Ex l, const char *lt, Ex r, const char *rt)
{
    int ints = is_int(lt) && is_int(rt);
    if (!strcmp(o, "/") && ints) return ex(fmt("_idiv(%s, %s)", l.s, r.s), P_POSTFIX);
    if (!strcmp(o, "%")) return ex(fmt("_imod(%s, %s)", l.s, r.s), P_POSTFIX);

    static const struct { const char *c, *py; int p; } ops[] = {
        { "+", "+", P_ADD }, { "-", "-", P_ADD }, { "*", "*", P_MUL }, { "/", "/", P_MUL },
        { "<<", "<<", P_SHIFT }, { ">>", ">>", P_SHIFT }, { "&", "&", P_BAND }, { "^", "^", P_BXOR },
        { "|", "|", P_BOR }, { "<", "<", P_CMP }, { ">", ">", P_CMP }, { "<=", "<=", P_CMP },
        { ">=", ">=", P_CMP }, { "==", "==", P_CMP }, { "!=", "!=", P_CMP },
    };
    for (size_t i = 0; i < sizeof ops / sizeof *ops; i++) {
        if (strcmp(o, ops[i].c)) continue;
        int p = ops[i].p;
        return ex(fmt("%s %s %s", wrap(l, p, p == P_CMP), ops[i].py, wrap(r, p, 1)), p);
    }
    return ex(fmt("%s ? %s", l.s, r.s), P_LOW);
}

static Ex assign_value(Node *n)
{
    Node *lhs = n->kid[0], *rhs = n->kid[1];
    if (!strcmp(n->op, "=")) return conv(lhs->ctype, rhs);
    char base[4] = { 0 };
    memcpy(base, n->op, strlen(n->op) - 1);
    const char *rt = is_dbl(lhs->ctype) || is_dbl(rhs->ctype) ? "double" : "int";
    Ex v = arith(base, expr(lhs, 0), lhs->ctype, expr(rhs, 0), rhs->ctype);
    if (is_int(lhs->ctype) && is_dbl(rt)) return ex(fmt("int(%s)", v.s), P_POSTFIX);
    return v;
}

static Ex expr(Node *n, int cond)
{
    switch (n->kind) {
    case N_INT: return ex(fmt("%lld", n->ival), P_ATOM);
    case N_DBL: {
        char *s = strdup(n->name);
        size_t l = strlen(s);
        if (l && (s[l - 1] == 'f' || s[l - 1] == 'F')) s[l - 1] = 0;
        return ex(s, P_ATOM);
    }
    case N_CHR:
        if (n->ival >= 32 && n->ival < 127 && n->ival != '\'' && n->ival != '\\')
            return ex(fmt("ord('%c')", (int)n->ival), P_POSTFIX);
        return ex(fmt("%lld", n->ival), P_ATOM);
    case N_STR: return ex(n->name, P_ATOM);
    case N_ID: return ex((char *)pyname(n->name), P_ATOM);
    case N_INDEX:
        return ex(fmt("%s[%s]", wrap(expr(n->kid[0], 0), P_POSTFIX, 0), expr(n->kid[1], 0).s), P_POSTFIX);
    case N_CALL: {
        Node *f = NULL;
        for (int i = 0; i < prog->n; i++)
            if (prog->kid[i]->kind == N_FUNC && !strcmp(prog->kid[i]->name, n->name)) f = prog->kid[i];
        char *args = strdup("");
        for (int i = 0; i < n->n; i++) {
            Ex a;
            if (f && !f->kid[0]->kid[i]->arr) a = conv(f->kid[0]->kid[i]->type, n->kid[i]);
            else if (boolish(n->kid[i])) a = conv("int", n->kid[i]);
            else a = expr(n->kid[i], 0);
            args = fmt("%s%s%s", args, i ? ", " : "", a.s);
        }
        return ex(fmt("%s(%s)", pyname(n->name), args), P_POSTFIX);
    }
    case N_UNOP: {
        Ex o = expr(n->kid[0], n->op[0] == '!');
        if (n->op[0] == '!') return ex(fmt("not %s", wrap(o, P_NOT, 0)), P_NOT);
        return ex(fmt("%s%s", n->op, wrap(o, P_UNARY, 0)), P_UNARY);
    }
    case N_CAST: {
        Ex o = expr(n->kid[0], 0);
        return ex(fmt("%s(%s)", is_dbl(n->type) ? "float" : "int", o.s), P_POSTFIX);
    }
    case N_TERNARY: {
        Ex c = expr(n->kid[0], 1), t = expr(n->kid[1], 0), f = expr(n->kid[2], 0);
        return ex(fmt("%s if %s else %s", wrap(t, P_TERN, 1), wrap(c, P_TERN, 1), wrap(f, P_TERN, 0)), P_TERN);
    }
    case N_BINOP: {
        Node *a = n->kid[0], *b = n->kid[1];
        if (!strcmp(n->op, "&&") || !strcmp(n->op, "||")) {
            int p = n->op[0] == '&' ? P_AND : P_OR;
            Ex l = expr(a, cond), r = expr(b, cond);
            if (!cond && !boolish(a)) l = ex(fmt("bool(%s)", l.s), P_POSTFIX);
            if (!cond && !boolish(b)) r = ex(fmt("bool(%s)", r.s), P_POSTFIX);
            return ex(fmt("%s %s %s", wrap(l, p, 0), p == P_AND ? "and" : "or", wrap(r, p, 1)), p);
        }
        return arith(n->op, expr(a, 0), a->ctype, expr(b, 0), b->ctype);
    }
    case N_ASSIGN: {
        Node *lhs = n->kid[0];
        Ex v = assign_value(n);
        if (lhs->kind == N_ID) return ex(fmt("(%s := %s)", pyname(lhs->name), v.s), P_ATOM);
        return ex(fmt("_set(%s, %s, %s)", expr(lhs->kid[0], 0).s, expr(lhs->kid[1], 0).s, v.s), P_POSTFIX);
    }
    case N_PREINC: case N_POSTINC: {
        Node *t = n->kid[0];
        int pre = n->kind == N_PREINC;
        char sign = n->op[0];
        if (t->kind == N_ID) {
            const char *nm = pyname(t->name);
            if (pre) return ex(fmt("(%s := %s %c 1)", nm, nm, sign), P_ATOM);
            return ex(fmt("(%s := %s %c 1) %c 1", nm, nm, sign, sign == '+' ? '-' : '+'), P_ADD);
        }
        return ex(fmt("%s(%s, %s, %s1)", pre ? "_preinc" : "_postinc",
                      expr(t->kid[0], 0).s, expr(t->kid[1], 0).s, sign == '-' ? "-" : ""), P_POSTFIX);
    }
    default:
        return ex("None", P_ATOM);
    }
}

static void expr_stmt(Node *e)
{
    if (e->kind == N_ASSIGN) {
        Node *lhs = e->kid[0], *rhs = e->kid[1];
        char *l = expr(lhs, 0).s;
        const char *o = e->op;
        int simple = strcmp(o, "=") && strcmp(o, "/=") && strcmp(o, "%=") &&
                     !(is_int(lhs->ctype) && is_dbl(rhs->ctype));
        if (simple) emit("%s %s %s", l, o, expr(rhs, 0).s);
        else if (!strcmp(o, "/=") && is_dbl(lhs->ctype)) emit("%s /= %s", l, expr(rhs, 0).s);
        else emit("%s = %s", l, assign_value(e).s);
        return;
    }
    if (e->kind == N_PREINC || e->kind == N_POSTINC) {
        emit("%s %c= 1", expr(e->kid[0], 0).s, e->op[0]);
        return;
    }
    emit("%s", expr(e, 0).s);
}

static void decl(Node *d)
{
    for (int i = 0; i < d->n; i++) {
        Node *v = d->kid[i];
        Node *init = v->n ? v->kid[0] : NULL;
        const char *zero = is_dbl(v->type) ? "0.0" : "0";
        const char *nm = pyname(v->name);
        if (v->arr) {
            if (!init) { emit("%s = [%s] * %d", nm, zero, v->arr); continue; }
            char *items = strdup("");
            for (int k = 0; k < init->n; k++) items = fmt("%s%s%s", items, k ? ", " : "", conv(v->type, init->kid[k]).s);
            if (v->arr > init->n) emit("%s = [%s] + [%s] * %d", nm, items, zero, v->arr - init->n);
            else emit("%s = [%s]", nm, items);
        } else {
            emit("%s = %s", nm, init ? conv(v->type, init).s : zero);
        }
    }
}

static void stmt(Node *n);

static void body(Node *n)
{
    int before = lines;
    ind++;
    stmt(n);
    if (lines == before) emit("pass");
    ind--;
}

static void loop_body(Kind k, Node *aux, Node *b)
{
    loops[nloops].kind = k;
    loops[nloops].n = aux;
    nloops++;
    body(b);
    nloops--;
}

static void cont_prefix(void)
{
    if (!nloops) return;
    Kind k = loops[nloops - 1].kind;
    Node *a = loops[nloops - 1].n;
    if (k == N_FOR && a) expr_stmt(a);
    if (k == N_DOWHILE) {
        emit("if %s:", fmt("not %s", wrap(expr(a, 1), P_NOT, 0)));
        ind++;
        emit("break");
        ind--;
    }
}

static void if_chain(Node *n, const char *kw)
{
    emit("%s %s:", kw, expr(n->kid[0], 1).s);
    body(n->kid[1]);
    Node *e = n->kid[2];
    if (!e) return;
    if (e->kind == N_IF) { if_chain(e, "elif"); return; }
    emit("else:");
    body(e);
}

static void stmt(Node *n)
{
    if (!n) return;
    switch (n->kind) {
    case N_BLOCK:
        for (int i = 0; i < n->n; i++) stmt(n->kid[i]);
        break;
    case N_DECL: decl(n); break;
    case N_EXPR: expr_stmt(n->kid[0]); break;
    case N_IF: if_chain(n, "if"); break;
    case N_WHILE:
        emit("while %s:", expr(n->kid[0], 1).s);
        loop_body(N_WHILE, NULL, n->kid[1]);
        break;
    case N_DOWHILE: {
        emit("while True:");
        loop_body(N_DOWHILE, n->kid[1], n->kid[0]);
        ind++;
        emit("if %s:", fmt("not %s", wrap(expr(n->kid[1], 1), P_NOT, 0)));
        ind++;
        emit("break");
        ind -= 2;
        break;
    }
    case N_FOR: {
        if (n->kid[0]) { if (n->kid[0]->kind == N_DECL) decl(n->kid[0]); else expr_stmt(n->kid[0]); }
        emit("while %s:", n->kid[1] ? expr(n->kid[1], 1).s : "True");
        loop_body(N_FOR, n->kid[2], n->kid[3]);
        if (n->kid[2]) { ind++; expr_stmt(n->kid[2]); ind--; }
        break;
    }
    case N_RETURN: {
        if (!n->kid[0]) emit("return");
        else emit("return %s", conv(curfn->type, n->kid[0]).s);
        break;
    }
    case N_BREAK: emit("break"); break;
    case N_CONTINUE:
        cont_prefix();
        emit("continue");
        break;
    default:
        break;
    }
}

static void collect_globals(Node *n, char **names, int *cnt)
{
    if (!n) return;
    if (n->kind == N_ID && n->global && n->ctype && !strchr(n->ctype, '[')) {
        for (int i = 0; i < *cnt; i++) if (!strcmp(names[i], n->name)) return;
        if (*cnt < 256) names[(*cnt)++] = n->name;
    }
    for (int i = 0; i < n->n; i++) collect_globals(n->kid[i], names, cnt);
}

static void func(Node *f)
{
    curfn = f;
    char *params = strdup("");
    Node *ps = f->kid[0];
    for (int i = 0; i < ps->n; i++) params = fmt("%s%s%s", params, i ? ", " : "", pyname(ps->kid[i]->name));
    fprintf(out, "\n\n");
    emit("def %s(%s):", pyname(f->name), params);
    ind++;
    char *g[256];
    int cnt = 0;
    collect_globals(f->kid[1], g, &cnt);
    if (cnt) {
        char *list = strdup("");
        for (int i = 0; i < cnt; i++) list = fmt("%s%s%s", list, i ? ", " : "", pyname(g[i]));
        emit("global %s", list);
    }
    ind--;
    body(f->kid[1]);
}

void gen_python(Node *p, FILE *f)
{
    char *text;
    size_t size;
    prog = p;
    out = open_memstream(&text, &size);
    ind = 0;
    int gl = 0;
    for (int i = 0; i < p->n; i++) {
        if (p->kid[i]->kind != N_DECL) continue;
        if (!gl++) fputs("\n", out);
        decl(p->kid[i]);
    }
    for (int i = 0; i < p->n; i++)
        if (p->kid[i]->kind == N_FUNC) func(p->kid[i]);
    fclose(out);

    int imod = uses(text, "_imod");
    fputs("import sys\n", f);
    if (uses(text, "printf")) fputs("import re\n", f);
    const char *math[] = { "sqrt", "pow", "fabs" };
    for (int i = 0; i < 3; i++)
        if (uses(text, math[i])) fprintf(f, "from math import %s\n", math[i]);
    for (size_t i = 0; i < sizeof helpers / sizeof *helpers; i++)
        if (uses(text, helpers[i].name) || (imod && !strcmp(helpers[i].name, "_idiv")))
            fprintf(f, "\n\n%s", helpers[i].code);
    fputs(text, f);
    fputs("\n\nif __name__ == \"__main__\":\n    sys.exit(main())\n", f);
    free(text);
}
