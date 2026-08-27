#ifndef KRATOS_CODEGEN_H
#define KRATOS_CODEGEN_H

#include "ast/ast.h"

#include <stdio.h>

/* Trascrive il programma in C su `out`. Restituisce 0 se ok. */
int codegen_emit_c(const AstNode *program, FILE *out);

#endif
