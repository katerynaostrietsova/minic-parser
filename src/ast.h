#ifndef AST_H
#define AST_H

typedef enum {
    N_PROGRAM, N_FUNC, N_PARAMS, N_PARAM, N_VAR_DECL, N_VAR,
    N_BLOCK, N_IF, N_WHILE, N_FOR, N_RETURN, N_BREAK, N_CONTINUE,
    N_EXPR_STMT, N_EMPTY,
    N_ASSIGN, N_BINOP, N_UNOP, N_INCDEC, N_CALL,
    N_ID, N_INT_LIT, N_FLOAT_LIT, N_STRING_LIT
} NodeKind;

typedef enum { T_UNKNOWN, T_INT, T_DOUBLE, T_VOID, T_STRING } TypeKind;

typedef struct Node {
    NodeKind     kind;
    int          line;

    char        *name;
    const char  *op;
    TypeKind     type;
    int          prefix;
    int          ival;
    double       dval;

    TypeKind     etype;
    char        *uname;
    int          is_global;

    struct Node **kids;
    int          nkids;
    int          cap;
} Node;

extern Node *ast_root;

Node *node_new(NodeKind kind, int line);
Node *node_add(Node *parent, Node *child);

Node *mk_binop (const char *op, Node *l, Node *r, int line);
Node *mk_unop  (const char *op, Node *e, int line);
Node *mk_assign(const char *op, char *name, Node *value, int line);
Node *mk_incdec(const char *op, char *name, int prefix, int line);
Node *mk_id    (char *name, int line);
Node *mk_int   (int v, int line);
Node *mk_float (double v, int line);
Node *mk_string(char *s, int line);

const char *kind_name(NodeKind k);
const char *type_name(TypeKind t);
void        ast_print(Node *n);
void        ast_free(Node *n);

#endif
