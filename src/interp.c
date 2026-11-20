#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interp.h"

typedef struct {
    TypeKind t;
    int      i;
    double   d;
} Value;

static Value v_int(int i)       { Value v = { T_INT, i, 0 };    return v; }
static Value v_double(double d) { Value v = { T_DOUBLE, 0, d }; return v; }

static double as_double(Value v) { return v.t == T_DOUBLE ? v.d : (double)v.i; }
static int    as_int(Value v)    { return v.t == T_DOUBLE ? (int)v.d : v.i; }
static int    truthy(Value v)    { return v.t == T_DOUBLE ? v.d != 0.0 : v.i != 0; }

static Value convert(Value v, TypeKind to)
{
    return to == T_DOUBLE ? v_double(as_double(v)) : v_int(as_int(v));
}

typedef struct Var {
    const char  *name;
    TypeKind     type;
    Value        val;
    struct Var  *next;
} Var;

typedef struct Env {
    Var         *vars;
    struct Env  *parent;
} Env;

static FILE *iout;
static Env  *globals;
static Node *program_root;
static int   call_depth;
#define MAX_CALL_DEPTH 10000

static void runtime_error(int line, const char *msg, const char *arg)
{
    fflush(iout);
    fprintf(stderr, "line %d: runtime error: %s%s\n", line, msg, arg ? arg : "");
    exit(1);
}

static Env *env_new(Env *parent)
{
    Env *e = calloc(1, sizeof(Env));
    e->parent = parent;
    return e;
}

static void env_free(Env *e)
{
    for (Var *v = e->vars, *nx; v; v = nx) { nx = v->next; free(v); }
    free(e);
}

static void env_define(Env *e, const char *name, TypeKind type, Value val)
{
    Var *v = calloc(1, sizeof(Var));
    v->name = name;
    v->type = type;
    v->val  = convert(val, type);
    v->next = e->vars;
    e->vars = v;
}

static Var *env_lookup(Env *e, const char *name)
{
    for (; e; e = e->parent)
        for (Var *v = e->vars; v; v = v->next)
            if (strcmp(v->name, name) == 0) return v;
    return NULL;
}

static Node *find_func(const char *name)
{
    for (int i = 0; i < program_root->nkids; i++) {
        Node *f = program_root->kids[i];
        if (f->kind == N_FUNC && strcmp(f->name, name) == 0) return f;
    }
    return NULL;
}

static Value eval(Node *n, Env *env);
static Value call_function(Node *f, Node *call, Env *env);

static int wrap_add(int a, int b) { return (int)((unsigned)a + (unsigned)b); }
static int wrap_sub(int a, int b) { return (int)((unsigned)a - (unsigned)b); }
static int wrap_mul(int a, int b) { return (int)((unsigned)a * (unsigned)b); }

static Value binop(const char *op, Value l, Value r, int line)
{
    int dbl = (l.t == T_DOUBLE || r.t == T_DOUBLE);

    switch (op[0]) {
    case '+': return dbl ? v_double(as_double(l) + as_double(r)) : v_int(wrap_add(l.i, r.i));
    case '-': return dbl ? v_double(as_double(l) - as_double(r)) : v_int(wrap_sub(l.i, r.i));
    case '*': return dbl ? v_double(as_double(l) * as_double(r)) : v_int(wrap_mul(l.i, r.i));
    case '/':
        if (dbl) return v_double(as_double(l) / as_double(r));
        if (r.i == 0) runtime_error(line, "division by zero", NULL);
        return v_int(l.i / r.i);
    case '%':
        if (r.i == 0) runtime_error(line, "modulo by zero", NULL);
        return v_int(l.i % r.i);
    }

    double a = as_double(l), b = as_double(r);
    if (strcmp(op, "<")  == 0) return v_int(dbl ? a <  b : l.i <  r.i);
    if (strcmp(op, ">")  == 0) return v_int(dbl ? a >  b : l.i >  r.i);
    if (strcmp(op, "<=") == 0) return v_int(dbl ? a <= b : l.i <= r.i);
    if (strcmp(op, ">=") == 0) return v_int(dbl ? a >= b : l.i >= r.i);
    if (strcmp(op, "==") == 0) return v_int(dbl ? a == b : l.i == r.i);
    if (strcmp(op, "!=") == 0) return v_int(dbl ? a != b : l.i != r.i);

    runtime_error(line, "unknown operator ", op);
    return v_int(0);
}

static Value builtin_printf(Node *call, Env *env)
{
    const char *f = call->kids[0]->name;
    int argi = 1, total = 0;

    for (const char *p = f; *p; p++) {
        if (*p != '%') { fputc(*p, iout); total++; continue; }
        if (p[1] == '%') { fputc('%', iout); total++; p++; continue; }

        char spec[32];
        int k = 0;
        spec[k++] = *p++;
        while (*p && strchr("-+ #0", *p) && k < 20) spec[k++] = *p++;
        while (*p >= '0' && *p <= '9' && k < 24)    spec[k++] = *p++;
        if (*p == '.') {
            spec[k++] = *p++;
            while (*p >= '0' && *p <= '9' && k < 28) spec[k++] = *p++;
        }
        while (*p == 'l' || *p == 'h') p++;
        char conv = *p;
        if (!conv) runtime_error(call->line, "incomplete format specifier in printf", NULL);
        spec[k++] = conv;
        spec[k] = '\0';

        if (argi >= call->nkids)
            runtime_error(call->line, "not enough arguments for format ", spec);
        Value v = eval(call->kids[argi++], env);

        switch (conv) {
        case 'd': case 'i': case 'x': case 'X': case 'o': case 'u': case 'c':
            total += fprintf(iout, spec, as_int(v));
            break;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
            total += fprintf(iout, spec, as_double(v));
            break;
        default:
            runtime_error(call->line, "unsupported printf conversion ", spec);
        }
    }
    return v_int(total);
}

static Value eval(Node *n, Env *env)
{
    switch (n->kind) {
    case N_INT_LIT:   return v_int(n->ival);
    case N_FLOAT_LIT: return v_double(n->dval);

    case N_ID:
        return env_lookup(env, n->name)->val;

    case N_ASSIGN: {
        Var  *var = env_lookup(env, n->name);
        Value val = eval(n->kids[0], env);
        if (n->op[0] != '=') {
            char op[2] = { n->op[0], '\0' };
            val = binop(op, var->val, val, n->line);
        }
        var->val = convert(val, var->type);
        return var->val;
    }

    case N_INCDEC: {
        Var  *var = env_lookup(env, n->name);
        Value old = var->val;
        Value one = v_int(1);
        var->val = convert(binop(n->op[0] == '+' ? "+" : "-", old, one, n->line), var->type);
        return n->prefix ? var->val : old;
    }

    case N_UNOP: {
        Value v = eval(n->kids[0], env);
        if (n->op[0] == '!') return v_int(!truthy(v));
        return v.t == T_DOUBLE ? v_double(-v.d) : v_int(wrap_sub(0, v.i));
    }

    case N_BINOP:
        if (strcmp(n->op, "&&") == 0)
            return v_int(truthy(eval(n->kids[0], env)) && truthy(eval(n->kids[1], env)));
        if (strcmp(n->op, "||") == 0)
            return v_int(truthy(eval(n->kids[0], env)) || truthy(eval(n->kids[1], env)));
        {
            Value l = eval(n->kids[0], env);
            Value r = eval(n->kids[1], env);
            return binop(n->op, l, r, n->line);
        }

    case N_CALL: {
        Node *f = find_func(n->name);
        if (!f) return builtin_printf(n, env);
        return call_function(f, n, env);
    }

    default:
        runtime_error(n->line, "cannot evaluate node ", kind_name(n->kind));
    }
    return v_int(0);
}

typedef enum { EX_NORMAL, EX_BREAK, EX_CONTINUE, EX_RETURN } ExecStatus;

static Value return_value;

static ExecStatus exec(Node *n, Env *env);

static void exec_var_decl(Node *n, Env *env)
{
    for (int i = 0; i < n->nkids; i++) {
        Node *v = n->kids[i];
        Value init = v->nkids > 0 ? eval(v->kids[0], env) : v_int(0);
        env_define(env, v->name, n->type, init);
    }
}

static ExecStatus exec_block(Node *n, Env *env)
{
    Env *local = env_new(env);
    ExecStatus st = EX_NORMAL;
    for (int i = 0; i < n->nkids && st == EX_NORMAL; i++)
        st = exec(n->kids[i], local);
    env_free(local);
    return st;
}

static ExecStatus exec(Node *n, Env *env)
{
    switch (n->kind) {
    case N_VAR_DECL:  exec_var_decl(n, env); return EX_NORMAL;
    case N_BLOCK:     return exec_block(n, env);
    case N_EXPR_STMT: eval(n->kids[0], env);  return EX_NORMAL;
    case N_EMPTY:     return EX_NORMAL;
    case N_BREAK:     return EX_BREAK;
    case N_CONTINUE:  return EX_CONTINUE;

    case N_RETURN:
        return_value = n->nkids > 0 ? eval(n->kids[0], env) : v_int(0);
        return EX_RETURN;

    case N_IF:
        if (truthy(eval(n->kids[0], env)))
            return exec(n->kids[1], env);
        if (n->nkids > 2)
            return exec(n->kids[2], env);
        return EX_NORMAL;

    case N_WHILE:
        while (truthy(eval(n->kids[0], env))) {
            ExecStatus st = exec(n->kids[1], env);
            if (st == EX_BREAK)  break;
            if (st == EX_RETURN) return st;
        }
        return EX_NORMAL;

    case N_FOR: {
        Env *local = env_new(env);
        Node *init = n->kids[0], *cond = n->kids[1], *step = n->kids[2], *body = n->kids[3];
        ExecStatus result = EX_NORMAL;

        if (init->kind == N_VAR_DECL)   exec_var_decl(init, local);
        else if (init->kind != N_EMPTY) eval(init, local);

        while (cond->kind == N_EMPTY || truthy(eval(cond, local))) {
            ExecStatus st = exec(body, local);
            if (st == EX_BREAK)  break;
            if (st == EX_RETURN) { result = st; break; }
            if (step->kind != N_EMPTY) eval(step, local);
        }
        env_free(local);
        return result;
    }

    default:
        runtime_error(n->line, "cannot execute node ", kind_name(n->kind));
    }
    return EX_NORMAL;
}

static Value call_function(Node *f, Node *call, Env *env)
{
    if (++call_depth > MAX_CALL_DEPTH)
        runtime_error(call->line, "stack overflow (recursion too deep) in ", f->name);

    Node  *params = f->kids[0];
    Value *args = malloc(sizeof(Value) * (params->nkids + 1));
    for (int i = 0; i < params->nkids; i++)
        args[i] = eval(call->kids[i], env);

    Env *fenv = env_new(globals);
    for (int i = 0; i < params->nkids; i++)
        env_define(fenv, params->kids[i]->name, params->kids[i]->type, args[i]);
    free(args);

    return_value = v_int(0);
    ExecStatus st = exec_block(f->kids[1], fenv);
    Value result = (st == EX_RETURN) ? return_value : v_int(0);
    env_free(fenv);

    call_depth--;
    return f->type == T_VOID ? v_int(0) : convert(result, f->type);
}

int interpret(Node *program)
{
    return interpret_to(program, stdout);
}

int interpret_to(Node *program, FILE *output)
{
    iout = output;
    program_root = program;
    globals = env_new(NULL);
    call_depth = 0;

    for (int i = 0; i < program->nkids; i++)
        if (program->kids[i]->kind == N_VAR_DECL)
            exec_var_decl(program->kids[i], globals);

    Node *main_fn = find_func("main");
    Node  fake_call = { .kind = N_CALL, .line = main_fn->line };
    Value rc = call_function(main_fn, &fake_call, globals);

    fflush(iout);
    env_free(globals);
    return as_int(rc);
}
