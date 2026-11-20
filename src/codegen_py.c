#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen_py.h"

static FILE *out;
static Node *root;
static int   indent;

#define MAX_LOOPS 256
static Node *loop_step[MAX_LOOPS];
static int   loop_top;

static char *fmt(const char *f, ...)
{
    va_list ap;
    va_start(ap, f);
    int len = vsnprintf(NULL, 0, f, ap);
    va_end(ap);
    char *s = malloc(len + 1);
    va_start(ap, f);
    vsnprintf(s, len + 1, f, ap);
    va_end(ap);
    return s;
}

static void emit(const char *f, ...)
{
    for (int i = 0; i < indent; i++) fputs("    ", out);
    va_list ap;
    va_start(ap, f);
    vfprintf(out, f, ap);
    va_end(ap);
    fputc('\n', out);
}

static const char *py_reserved[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "class", "def", "del", "elif", "except", "finally", "from", "global",
    "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass",
    "raise", "try", "with", "yield",
    "int", "float", "bool", "print", "sys", "abs", "len", "str", "range",
    "list", "dict", "type", "id", "input", "min", "max", "sum",
    NULL
};

static char *py_name(const char *name)
{
    for (int i = 0; py_reserved[i]; i++)
        if (strcmp(name, py_reserved[i]) == 0)
            return fmt("%s_", name);
    if (name[0] == '_')
        return fmt("u%s", name);
    return strdup(name);
}

static char *var_name(Node *n)
{
    return py_name(n->uname ? n->uname : n->name);
}

static Node *find_func(const char *name)
{
    for (int i = 0; i < root->nkids; i++)
        if (root->kids[i]->kind == N_FUNC && strcmp(root->kids[i]->name, name) == 0)
            return root->kids[i];
    return NULL;
}

enum {
    P_WALRUS = 0,
    P_OR     = 1,
    P_AND    = 2,
    P_NOT    = 3,
    P_CMP    = 4,
    P_ADD    = 6,
    P_MUL    = 7,
    P_UNARY  = 8,
    P_ATOM   = 10
};

static char *gen_expr(Node *n, int *prec, int cond);

static char *gen_sub(Node *n, int min, int cond)
{
    int p;
    char *s = gen_expr(n, &p, cond);
    if (p < min) {
        char *t = fmt("(%s)", s);
        free(s);
        return t;
    }
    return s;
}

static char *convert(char *e, int *prec, TypeKind from, TypeKind to)
{
    if (from == to || to == T_VOID || to == T_UNKNOWN) return e;
    char *r = NULL;
    if (to == T_DOUBLE && from == T_INT) r = fmt("float(%s)", e);
    if (to == T_INT && from == T_DOUBLE) r = fmt("int(%s)", e);
    if (!r) return e;
    free(e);
    *prec = P_ATOM;
    return r;
}

static char *py_string(const char *s)
{
    size_t cap = strlen(s) * 2 + 3;
    char *r = malloc(cap), *p = r;
    *p++ = '"';
    for (; *s; s++) {
        switch (*s) {
        case '\n': *p++ = '\\'; *p++ = 'n';  break;
        case '\t': *p++ = '\\'; *p++ = 't';  break;
        case '"':  *p++ = '\\'; *p++ = '"';  break;
        case '\\': *p++ = '\\'; *p++ = '\\'; break;
        default:   *p++ = *s;
        }
    }
    *p++ = '"';
    *p = '\0';
    return r;
}

static char *py_float(double d)
{
    char buf[64];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    if (!strpbrk(buf, ".eni")) strcat(buf, ".0");
    return strdup(buf);
}

static char *gen_compound(Node *n, const char *op, Node *value, TypeKind var_t)
{
    char *v = var_name(n);
    char *rhs = gen_sub(value, P_MUL + 1, 0);
    TypeKind vt = value->etype;
    int dbl = (var_t == T_DOUBLE || vt == T_DOUBLE);
    char *r;

    if (op[0] == '/' && !dbl)      r = fmt("_cdiv(%s, %s)", v, rhs);
    else if (op[0] == '%')         r = fmt("_cmod(%s, %s)", v, rhs);
    else                           r = fmt("%s %c %s", v, op[0], rhs);

    free(v);
    free(rhs);
    int p = P_ADD;
    return convert(r, &p, dbl ? T_DOUBLE : T_INT, var_t);
}

static char *gen_call(Node *n)
{
    Node *f = find_func(n->name);
    size_t cap = 64;
    char *args = calloc(1, cap);

    for (int i = 0; i < n->nkids; i++) {
        int p;
        char *a = gen_expr(n->kids[i], &p, 0);
        if (f)
            a = convert(a, &p, n->kids[i]->etype, f->kids[0]->kids[i]->type);
        size_t need = strlen(args) + strlen(a) + 3;
        if (need > cap) { cap = need * 2; args = realloc(args, cap); }
        if (i > 0) strcat(args, ", ");
        strcat(args, a);
        free(a);
    }

    char *r;
    if (!f) {
        r = fmt("_printf(%s)", args);
    } else {
        char *fn = py_name(n->name);
        r = fmt("%s(%s)", fn, args);
        free(fn);
    }
    free(args);
    return r;
}

static char *gen_expr(Node *n, int *prec, int cond)
{
    *prec = P_ATOM;

    switch (n->kind) {
    case N_INT_LIT:    return fmt("%d", n->ival);
    case N_FLOAT_LIT:  return py_float(n->dval);
    case N_STRING_LIT: return py_string(n->name);
    case N_ID:         return var_name(n);
    case N_CALL:       return gen_call(n);

    case N_ASSIGN: {
        char *v = var_name(n), *val, *r;
        if (n->op[0] == '=') {
            int p;
            val = gen_expr(n->kids[0], &p, 0);
            val = convert(val, &p, n->kids[0]->etype, n->etype);
        } else {
            val = gen_compound(n, n->op, n->kids[0], n->etype);
        }
        r = fmt("(%s := %s)", v, val);
        free(v);
        free(val);
        return r;
    }

    case N_INCDEC: {
        char *v = var_name(n), *r;
        char sign = n->op[0], undo = (sign == '+') ? '-' : '+';
        if (n->prefix) r = fmt("(%s := %s %c 1)", v, v, sign);
        else           r = fmt("((%s := %s %c 1) %c 1)", v, v, sign, undo);
        free(v);
        return r;
    }

    case N_UNOP: {
        char *e, *r;
        if (n->op[0] == '!') {
            e = gen_sub(n->kids[0], P_NOT, 1);
            r = fmt("not %s", e);
            *prec = P_NOT;
        } else {
            e = gen_sub(n->kids[0], n->kids[0]->kind == N_UNOP ? P_ATOM : P_UNARY, 0);
            r = fmt("-%s", e);
            *prec = P_UNARY;
        }
        free(e);
        return r;
    }

    case N_BINOP: {
        const char *op = n->op;
        char *l, *r, *res;

        if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
            int is_and = (op[0] == '&');
            int p = is_and ? P_AND : P_OR;
            l = gen_sub(n->kids[0], p, 1);
            r = gen_sub(n->kids[1], p + 1, 1);
            res = fmt("%s %s %s", l, is_and ? "and" : "or", r);
            free(l); free(r);
            if (cond) { *prec = p; return res; }
            char *b = fmt("bool(%s)", res);
            free(res);
            return b;
        }

        if (strchr("<>=!", op[0])) {
            l = gen_sub(n->kids[0], P_CMP + 1, 0);
            r = gen_sub(n->kids[1], P_CMP + 1, 0);
            res = fmt("%s %s %s", l, op, r);
            *prec = P_CMP;
            free(l); free(r);
            return res;
        }

        if ((op[0] == '/' && n->etype == T_INT) || op[0] == '%') {
            l = gen_sub(n->kids[0], P_WALRUS + 1, 0);
            r = gen_sub(n->kids[1], P_WALRUS + 1, 0);
            res = fmt("%s(%s, %s)", op[0] == '/' ? "_cdiv" : "_cmod", l, r);
            free(l); free(r);
            return res;
        }

        int p = (op[0] == '+' || op[0] == '-') ? P_ADD : P_MUL;
        l = gen_sub(n->kids[0], p, 0);
        r = gen_sub(n->kids[1], p + 1, 0);
        res = fmt("%s %s %s", l, op, r);
        *prec = p;
        free(l); free(r);
        return res;
    }

    default:
        return fmt("None");
    }
}

static char *gen_cond(Node *n)
{
    int p;
    return gen_expr(n, &p, 1);
}

static int gen_stmt(Node *n);

static int gen_expr_stmt(Node *e)
{
    if (e->kind == N_ASSIGN) {
        char *v = var_name(e);
        int p;
        if (e->op[0] == '=') {
            char *val = gen_expr(e->kids[0], &p, 0);
            val = convert(val, &p, e->kids[0]->etype, e->etype);
            emit("%s = %s", v, val);
            free(val);
        } else if (strchr("+-*", e->op[0]) &&
                   !(e->etype == T_INT && e->kids[0]->etype == T_DOUBLE)) {
            char *val = gen_sub(e->kids[0], P_WALRUS + 1, 0);
            emit("%s %s %s", v, e->op, val);
            free(val);
        } else {
            char *val = gen_compound(e, e->op, e->kids[0], e->etype);
            emit("%s = %s", v, val);
            free(val);
        }
        free(v);
        return 1;
    }
    if (e->kind == N_INCDEC) {
        char *v = var_name(e);
        emit("%s %c= 1", v, e->op[0]);
        free(v);
        return 1;
    }
    int p;
    char *s = gen_expr(e, &p, 0);
    emit("%s", s);
    free(s);
    return 1;
}

static int gen_var_decl(Node *n)
{
    for (int i = 0; i < n->nkids; i++) {
        Node *v = n->kids[i];
        char *name = var_name(v);
        if (v->nkids > 0) {
            int p;
            char *init = gen_expr(v->kids[0], &p, 0);
            init = convert(init, &p, v->kids[0]->etype, n->type);
            emit("%s = %s", name, init);
            free(init);
        } else {
            emit("%s = %s", name, n->type == T_DOUBLE ? "0.0" : "0");
        }
        free(name);
    }
    return n->nkids;
}

static void gen_body(Node *n)
{
    indent++;
    if (gen_stmt(n) == 0) emit("pass");
    indent--;
}

static int gen_stmt(Node *n)
{
    int count = 0;

    switch (n->kind) {
    case N_VAR_DECL:
        return gen_var_decl(n);

    case N_BLOCK:
        for (int i = 0; i < n->nkids; i++)
            count += gen_stmt(n->kids[i]);
        return count;

    case N_EXPR_STMT:
        return gen_expr_stmt(n->kids[0]);

    case N_EMPTY:
        return 0;

    case N_IF: {
        char *c = gen_cond(n->kids[0]);
        emit("if %s:", c);
        free(c);
        gen_body(n->kids[1]);
        Node *els = n->nkids > 2 ? n->kids[2] : NULL;
        while (els && els->kind == N_IF) {
            c = gen_cond(els->kids[0]);
            emit("elif %s:", c);
            free(c);
            gen_body(els->kids[1]);
            els = els->nkids > 2 ? els->kids[2] : NULL;
        }
        if (els) {
            emit("else:");
            gen_body(els);
        }
        return 1;
    }

    case N_WHILE: {
        char *c = gen_cond(n->kids[0]);
        emit("while %s:", c);
        free(c);
        loop_step[loop_top++] = NULL;
        gen_body(n->kids[1]);
        loop_top--;
        return 1;
    }

    case N_FOR: {
        Node *init = n->kids[0], *cond = n->kids[1], *step = n->kids[2], *body = n->kids[3];
        if (init->kind == N_VAR_DECL)   count += gen_var_decl(init);
        else if (init->kind != N_EMPTY) count += gen_expr_stmt(init);

        if (cond->kind == N_EMPTY) {
            emit("while True:");
        } else {
            char *c = gen_cond(cond);
            emit("while %s:", c);
            free(c);
        }
        loop_step[loop_top++] = (step->kind != N_EMPTY) ? step : NULL;
        indent++;
        int body_lines = gen_stmt(body);
        if (step->kind != N_EMPTY) body_lines += gen_expr_stmt(step);
        if (body_lines == 0) emit("pass");
        indent--;
        loop_top--;
        return count + 1;
    }

    case N_RETURN:
        if (n->nkids == 0) {
            emit("return");
        } else {
            int p;
            char *e = gen_expr(n->kids[0], &p, 0);
            e = convert(e, &p, n->kids[0]->etype, n->etype);
            emit("return %s", e);
            free(e);
        }
        return 1;

    case N_BREAK:
        emit("break");
        return 1;

    case N_CONTINUE:
        if (loop_top > 0 && loop_step[loop_top - 1])
            gen_expr_stmt(loop_step[loop_top - 1]);
        emit("continue");
        return 1;

    default:
        return 0;
    }
}

static void collect_globals(Node *n, char **names, int *count)
{
    if ((n->kind == N_ASSIGN || n->kind == N_INCDEC) && n->is_global) {
        char *v = var_name(n);
        for (int i = 0; i < *count; i++)
            if (strcmp(names[i], v) == 0) { free(v); v = NULL; break; }
        if (v && *count < 256) names[(*count)++] = v;
        else free(v);
    }
    for (int i = 0; i < n->nkids; i++)
        collect_globals(n->kids[i], names, count);
}

static void mark_returns(Node *n, TypeKind t)
{
    if (n->kind == N_RETURN) n->etype = t;
    for (int i = 0; i < n->nkids; i++)
        mark_returns(n->kids[i], t);
}

static void gen_func(Node *f)
{
    Node *params = f->kids[0];
    char plist[1024] = "";
    for (int i = 0; i < params->nkids; i++) {
        char *p = var_name(params->kids[i]);
        if (i > 0) strcat(plist, ", ");
        strncat(plist, p, sizeof plist - strlen(plist) - 1);
        free(p);
    }

    char *fn = py_name(f->name);
    emit("def %s(%s):", fn, plist);
    free(fn);
    indent++;

    char *globals[256];
    int ng = 0;
    collect_globals(f->kids[1], globals, &ng);
    if (ng > 0) {
        char line[1024] = "global ";
        for (int i = 0; i < ng; i++) {
            if (i > 0) strcat(line, ", ");
            strncat(line, globals[i], sizeof line - strlen(line) - 1);
            free(globals[i]);
        }
        emit("%s", line);
    }

    mark_returns(f->kids[1], f->type);
    int lines = gen_stmt(f->kids[1]);
    if (lines == 0 && ng == 0) emit("pass");
    indent--;
    emit("");
    emit("");
}

void codegen_python(Node *program, const char *source_name, FILE *o)
{
    (void)source_name;
    out = o;
    root = program;
    indent = 0;
    loop_top = 0;

    emit("import sys");
    emit("");
    emit("");
    emit("def _cdiv(a, b):");
    emit("    if b == 0:");
    emit("        raise ZeroDivisionError(\"division by zero\")");
    emit("    q = abs(a) // abs(b)");
    emit("    return q if (a >= 0) == (b >= 0) else -q");
    emit("");
    emit("");
    emit("def _cmod(a, b):");
    emit("    return a - _cdiv(a, b) * b");
    emit("");
    emit("");
    emit("def _printf(fmt, *args):");
    emit("    s = fmt %% args");
    emit("    sys.stdout.write(s)");
    emit("    return len(s)");
    emit("");
    emit("");

    int has_globals = 0;
    for (int i = 0; i < program->nkids; i++) {
        if (program->kids[i]->kind == N_VAR_DECL) {
            has_globals = 1;
            gen_var_decl(program->kids[i]);
        }
    }
    if (has_globals) { emit(""); emit(""); }

    for (int i = 0; i < program->nkids; i++)
        if (program->kids[i]->kind == N_FUNC)
            gen_func(program->kids[i]);

    emit("if __name__ == \"__main__\":");
    emit("    sys.exit(main())");
}
