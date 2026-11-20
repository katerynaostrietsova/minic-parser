#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

Node *ast_root = NULL;

Node *node_new(NodeKind kind, int line)
{
    Node *n = calloc(1, sizeof(Node));
    if (!n) { perror("calloc"); exit(1); }
    n->kind = kind;
    n->line = line;
    return n;
}

Node *node_add(Node *parent, Node *child)
{
    if (parent->nkids == parent->cap) {
        parent->cap = parent->cap ? parent->cap * 2 : 4;
        parent->kids = realloc(parent->kids, parent->cap * sizeof(Node *));
        if (!parent->kids) { perror("realloc"); exit(1); }
    }
    parent->kids[parent->nkids++] = child;
    return parent;
}

Node *mk_binop(const char *op, Node *l, Node *r, int line)
{
    Node *n = node_new(N_BINOP, line);
    n->op = op;
    node_add(n, l);
    node_add(n, r);
    return n;
}

Node *mk_unop(const char *op, Node *e, int line)
{
    Node *n = node_new(N_UNOP, line);
    n->op = op;
    node_add(n, e);
    return n;
}

Node *mk_assign(const char *op, char *name, Node *value, int line)
{
    Node *n = node_new(N_ASSIGN, line);
    n->op = op;
    n->name = name;
    node_add(n, value);
    return n;
}

Node *mk_incdec(const char *op, char *name, int prefix, int line)
{
    Node *n = node_new(N_INCDEC, line);
    n->op = op;
    n->name = name;
    n->prefix = prefix;
    return n;
}

Node *mk_id(char *name, int line)
{
    Node *n = node_new(N_ID, line);
    n->name = name;
    return n;
}

Node *mk_int(int v, int line)
{
    Node *n = node_new(N_INT_LIT, line);
    n->ival = v;
    return n;
}

Node *mk_float(double v, int line)
{
    Node *n = node_new(N_FLOAT_LIT, line);
    n->dval = v;
    return n;
}

Node *mk_string(char *s, int line)
{
    Node *n = node_new(N_STRING_LIT, line);
    n->name = s;
    return n;
}

const char *kind_name(NodeKind k)
{
    static const char *names[] = {
        "PROGRAM", "FUNC", "PARAMS", "PARAM", "VAR_DECL", "VAR",
        "BLOCK", "IF", "WHILE", "FOR", "RETURN", "BREAK", "CONTINUE",
        "EXPR_STMT", "EMPTY",
        "ASSIGN", "BINOP", "UNOP", "INCDEC", "CALL",
        "ID", "INT_LIT", "FLOAT_LIT", "STRING_LIT"
    };
    return names[k];
}

const char *type_name(TypeKind t)
{
    switch (t) {
        case T_UNKNOWN: return "?";
        case T_INT:     return "int";
        case T_DOUBLE:  return "double";
        case T_VOID:    return "void";
        case T_STRING:  return "string";
    }
    return "?";
}

static void print_escaped(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n')      printf("\\n");
        else if (*s == '\t') printf("\\t");
        else if (*s == '"')  printf("\\\"");
        else                 putchar(*s);
    }
}

static void print_label(Node *n)
{
    printf("%s", kind_name(n->kind));
    switch (n->kind) {
        case N_FUNC:
        case N_PARAM:      printf(" %s %s", type_name(n->type), n->name); break;
        case N_VAR_DECL:   printf(" %s", type_name(n->type)); break;
        case N_VAR:
        case N_ID:
        case N_CALL:       printf(" %s", n->name); break;
        case N_ASSIGN:     printf(" %s %s", n->name, n->op); break;
        case N_BINOP:
        case N_UNOP:       printf(" %s", n->op); break;
        case N_INCDEC:     if (n->prefix) printf(" %s%s", n->op, n->name);
                           else           printf(" %s%s", n->name, n->op);
                           break;
        case N_INT_LIT:    printf(" %d", n->ival); break;
        case N_FLOAT_LIT:
                           if (n->dval == (long long)n->dval) printf(" %.1f", n->dval);
                           else                               printf(" %g", n->dval);
                           break;
        case N_STRING_LIT: printf(" \""); print_escaped(n->name); printf("\""); break;
        default: break;
    }
    if (n->uname && n->name && strcmp(n->uname, n->name) != 0)
        printf(" [%s]", n->uname);
    if (n->etype != T_UNKNOWN)
        printf(" : %s", type_name(n->etype));
    printf("   (line %d)\n", n->line);
}

static void print_rec(Node *n, char *prefix, int is_last, int is_root)
{
    if (!is_root) printf("%s%s", prefix, is_last ? "└── " : "├── ");
    print_label(n);

    size_t len = strlen(prefix);
    char *child_prefix = malloc(len + 8);
    strcpy(child_prefix, prefix);
    if (!is_root) strcat(child_prefix, is_last ? "    " : "│   ");

    for (int i = 0; i < n->nkids; i++)
        print_rec(n->kids[i], child_prefix, i == n->nkids - 1, 0);

    free(child_prefix);
}

void ast_print(Node *n)
{
    char empty[1] = "";
    if (n) print_rec(n, empty, 1, 1);
}

void ast_free(Node *n)
{
    if (!n) return;
    for (int i = 0; i < n->nkids; i++)
        ast_free(n->kids[i]);
    free(n->kids);
    free(n->name);
    free(n->uname);
    free(n);
}
