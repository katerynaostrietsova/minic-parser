#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"

typedef enum { S_VAR, S_FUNC } SymKind;

typedef struct Sym {
    char        *name;
    SymKind      kind;
    TypeKind     type;
    Node        *decl;
    char        *uname;
    int          is_global;
    struct Sym  *next;
} Sym;

typedef struct Scope {
    Sym          *syms;
    struct Scope *parent;
} Scope;

static Scope *cur_scope;
static Scope *global_scope;
static int    errors;
static int    loop_depth;
static Node  *cur_func;

typedef struct NameCount { char *name; int count; struct NameCount *next; } NameCount;
static NameCount *name_counts;

static void error(int line, const char *fmt, const char *a, const char *b)
{
    fprintf(stderr, "line %d: semantic error: ", line);
    fprintf(stderr, fmt, a, b);
    fputc('\n', stderr);
    errors++;
}

static void scope_push(void)
{
    Scope *s = calloc(1, sizeof(Scope));
    s->parent = cur_scope;
    cur_scope = s;
}

static void scope_pop(void)
{
    Scope *s = cur_scope;
    cur_scope = s->parent;
    for (Sym *p = s->syms, *nx; p; p = nx) {
        nx = p->next;
        free(p->uname);
        free(p);
    }
    free(s);
}

static Sym *lookup_in(Scope *s, const char *name)
{
    for (Sym *p = s->syms; p; p = p->next)
        if (strcmp(p->name, name) == 0) return p;
    return NULL;
}

static Sym *lookup(const char *name)
{
    for (Scope *s = cur_scope; s; s = s->parent) {
        Sym *p = lookup_in(s, name);
        if (p) return p;
    }
    return NULL;
}

static char *make_uname(const char *name)
{
    NameCount *nc;
    for (nc = name_counts; nc; nc = nc->next)
        if (strcmp(nc->name, name) == 0) break;
    if (!nc) {
        nc = calloc(1, sizeof(NameCount));
        nc->name = strdup(name);
        nc->next = name_counts;
        name_counts = nc;
    }
    if (lookup(name) == NULL)
        return strdup(name);
    char buf[256];
    do {
        nc->count++;
        snprintf(buf, sizeof buf, "%s_%d", name, nc->count + 1);
    } while (lookup(buf) != NULL);
    return strdup(buf);
}

static Sym *declare(const char *name, SymKind kind, TypeKind type, Node *decl, int line)
{
    if (lookup_in(cur_scope, name)) {
        error(line, "redeclaration of '%s'%s", name, "");
        return NULL;
    }
    Sym *s = calloc(1, sizeof(Sym));
    s->name = (char *)name;
    s->kind = kind;
    s->type = type;
    s->decl = decl;
    s->is_global = (cur_scope == global_scope);
    if (kind == S_VAR) s->uname = make_uname(name);
    s->next = cur_scope->syms;
    cur_scope->syms = s;
    return s;
}

static TypeKind check_expr(Node *n);

static TypeKind check_numeric(Node *n, const char *what)
{
    TypeKind t = check_expr(n);
    if (t == T_VOID)
        error(n->line, "void value used in %s%s", what, "");
    else if (t == T_STRING)
        error(n->line, "string literal used in %s%s", what, " (only allowed as printf format)");
    return t;
}

static Sym *resolve_var(Node *n)
{
    Sym *s = lookup(n->name);
    if (!s) {
        error(n->line, "'%s' undeclared%s", n->name, "");
        return NULL;
    }
    if (s->kind != S_VAR) {
        error(n->line, "'%s' is a function, not a variable%s", n->name, "");
        return NULL;
    }
    n->uname = strdup(s->uname);
    n->is_global = s->is_global;
    return s;
}

static TypeKind check_call(Node *n)
{
    if (strcmp(n->name, "printf") == 0 && !lookup("printf")->decl) {
        if (n->nkids == 0 || n->kids[0]->kind != N_STRING_LIT) {
            error(n->line, "first argument of printf must be a string literal%s%s", "", "");
        }
        for (int i = 0; i < n->nkids; i++) {
            if (i == 0 && n->kids[0]->kind == N_STRING_LIT)
                n->kids[0]->etype = T_STRING;
            else
                check_numeric(n->kids[i], "printf argument");
        }
        return T_INT;
    }

    Sym *s = lookup(n->name);
    if (!s) {
        error(n->line, "function '%s' undeclared%s", n->name, "");
        for (int i = 0; i < n->nkids; i++) check_expr(n->kids[i]);
        return T_INT;
    }
    if (s->kind != S_FUNC) {
        error(n->line, "'%s' is not a function%s", n->name, "");
        return T_INT;
    }

    Node *params = s->decl->kids[0];
    if (n->nkids != params->nkids) {
        char buf[64];
        snprintf(buf, sizeof buf, " (expected %d, got %d)", params->nkids, n->nkids);
        error(n->line, "wrong number of arguments to '%s'%s", n->name, buf);
    }
    for (int i = 0; i < n->nkids; i++)
        check_numeric(n->kids[i], "function argument");
    return s->type;
}

static TypeKind check_expr(Node *n)
{
    TypeKind t = T_INT;

    switch (n->kind) {
    case N_INT_LIT:    t = T_INT;    break;
    case N_FLOAT_LIT:  t = T_DOUBLE; break;
    case N_STRING_LIT: t = T_STRING; break;

    case N_ID: {
        Sym *s = resolve_var(n);
        t = s ? s->type : T_INT;
        break;
    }

    case N_ASSIGN: {
        Sym *s = resolve_var(n);
        TypeKind vt = check_numeric(n->kids[0], "assignment");
        t = s ? s->type : T_INT;
        if (strcmp(n->op, "%=") == 0 && (t == T_DOUBLE || vt == T_DOUBLE))
            error(n->line, "invalid operands to '%s' (operands must be int)%s", "%=", "");
        break;
    }

    case N_INCDEC: {
        Sym *s = resolve_var(n);
        t = s ? s->type : T_INT;
        break;
    }

    case N_UNOP: {
        TypeKind et = check_numeric(n->kids[0], "unary expression");
        t = (strcmp(n->op, "!") == 0) ? T_INT : et;
        break;
    }

    case N_BINOP: {
        TypeKind l = check_numeric(n->kids[0], "expression");
        TypeKind r = check_numeric(n->kids[1], "expression");
        const char *op = n->op;
        if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
            strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
            t = (l == T_DOUBLE || r == T_DOUBLE) ? T_DOUBLE : T_INT;
        } else if (strcmp(op, "%") == 0) {
            if (l == T_DOUBLE || r == T_DOUBLE)
                error(n->line, "invalid operands to '%s' (operands must be int)%s", "%", "");
            t = T_INT;
        } else {
            t = T_INT;
        }
        break;
    }

    case N_CALL:
        t = check_call(n);
        break;

    default:
        error(n->line, "unexpected node %s in expression%s", kind_name(n->kind), "");
    }

    n->etype = t;
    return t;
}

static int is_const_expr(Node *n)
{
    switch (n->kind) {
    case N_INT_LIT: case N_FLOAT_LIT: return 1;
    case N_UNOP: case N_BINOP:
        for (int i = 0; i < n->nkids; i++)
            if (!is_const_expr(n->kids[i])) return 0;
        return 1;
    default: return 0;
    }
}

static void check_stmt(Node *n);

static void check_var_decl(Node *n)
{
    if (n->type == T_VOID) {
        error(n->line, "variable declared void%s%s", "", "");
        return;
    }
    for (int i = 0; i < n->nkids; i++) {
        Node *v = n->kids[i];
        if (v->nkids > 0) {
            check_numeric(v->kids[0], "initializer");
            if (cur_scope == global_scope && !is_const_expr(v->kids[0]))
                error(v->line, "initializer of global '%s' is not a constant%s", v->name, "");
        }
        Sym *s = declare(v->name, S_VAR, n->type, v, v->line);
        if (s) {
            v->uname = strdup(s->uname);
            v->is_global = s->is_global;
        }
        v->etype = n->type;
    }
}

static void check_block(Node *n, int new_scope)
{
    if (new_scope) scope_push();
    for (int i = 0; i < n->nkids; i++)
        check_stmt(n->kids[i]);
    if (new_scope) scope_pop();
}

static void check_stmt(Node *n)
{
    switch (n->kind) {
    case N_VAR_DECL:
        check_var_decl(n);
        break;

    case N_BLOCK:
        check_block(n, 1);
        break;

    case N_EXPR_STMT:
        check_expr(n->kids[0]);
        break;

    case N_EMPTY:
        break;

    case N_IF:
        check_numeric(n->kids[0], "if condition");
        check_stmt(n->kids[1]);
        if (n->nkids > 2) check_stmt(n->kids[2]);
        break;

    case N_WHILE:
        check_numeric(n->kids[0], "while condition");
        loop_depth++;
        check_stmt(n->kids[1]);
        loop_depth--;
        break;

    case N_FOR:
        scope_push();
        if (n->kids[0]->kind == N_VAR_DECL) check_var_decl(n->kids[0]);
        else if (n->kids[0]->kind != N_EMPTY) check_expr(n->kids[0]);
        if (n->kids[1]->kind != N_EMPTY) check_numeric(n->kids[1], "for condition");
        if (n->kids[2]->kind != N_EMPTY) check_expr(n->kids[2]);
        loop_depth++;
        check_stmt(n->kids[3]);
        loop_depth--;
        scope_pop();
        break;

    case N_RETURN:
        if (cur_func->type == T_VOID && n->nkids > 0)
            error(n->line, "'return' with a value in void function '%s'%s", cur_func->name, "");
        else if (cur_func->type != T_VOID && n->nkids == 0)
            error(n->line, "'return' without a value in function '%s' returning non-void%s",
                  cur_func->name, "");
        if (n->nkids > 0) check_numeric(n->kids[0], "return");
        break;

    case N_BREAK:
        if (loop_depth == 0) error(n->line, "'break' not within a loop%s%s", "", "");
        break;

    case N_CONTINUE:
        if (loop_depth == 0) error(n->line, "'continue' not within a loop%s%s", "", "");
        break;

    default:
        error(n->line, "unexpected node %s in statement%s", kind_name(n->kind), "");
    }
}

static void check_func(Node *f)
{
    cur_func = f;
    Node *params = f->kids[0];
    Node *body   = f->kids[1];

    scope_push();
    for (int i = 0; i < params->nkids; i++) {
        Node *p = params->kids[i];
        if (p->type == T_VOID) {
            error(p->line, "parameter '%s' declared void%s", p->name, "");
            continue;
        }
        Sym *s = declare(p->name, S_VAR, p->type, p, p->line);
        if (s) p->uname = strdup(s->uname);
    }
    check_block(body, 0);
    scope_pop();
    cur_func = NULL;
}

int semantic_check(Node *program)
{
    errors = 0;
    loop_depth = 0;
    cur_scope = NULL;
    scope_push();
    global_scope = cur_scope;

    declare("printf", S_FUNC, T_INT, NULL, 0);

    for (int i = 0; i < program->nkids; i++) {
        Node *e = program->kids[i];
        if (e->kind == N_FUNC)
            declare(e->name, S_FUNC, e->type, e, e->line);
    }

    for (int i = 0; i < program->nkids; i++) {
        Node *e = program->kids[i];
        if (e->kind == N_FUNC) check_func(e);
        else                   check_var_decl(e);
    }

    Sym *m = lookup_in(global_scope, "main");
    if (!m || m->kind != S_FUNC)
        error(program->line, "function 'main' is not defined%s%s", "", "");
    else if (m->type != T_INT || m->decl->kids[0]->nkids != 0)
        error(m->decl->line, "'main' must be declared as 'int main()'%s%s", "", "");

    scope_pop();
    global_scope = NULL;
    for (NameCount *nc = name_counts, *nx; nc; nc = nx) {
        nx = nc->next;
        free(nc->name);
        free(nc);
    }
    name_counts = NULL;
    return errors;
}
