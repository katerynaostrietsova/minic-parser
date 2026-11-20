#ifndef AST_JSON_H
#define AST_JSON_H

#include <stdio.h>
#include "ast.h"

void ast_export_json(Node *program, const char *path, FILE *out);

#endif
