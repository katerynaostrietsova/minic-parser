#ifndef CODEGEN_PY_H
#define CODEGEN_PY_H

#include <stdio.h>
#include "ast.h"

void codegen_python(Node *program, const char *source_name, FILE *out);

#endif
