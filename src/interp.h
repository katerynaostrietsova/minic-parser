#ifndef INTERP_H
#define INTERP_H

#include <stdio.h>
#include "ast.h"

int interpret(Node *program);

int interpret_to(Node *program, FILE *output);

#endif
