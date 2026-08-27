#ifndef KRATOS_INTERP_H
#define KRATOS_INTERP_H

#include "ast/ast.h"

/* Esegue il programma. Chiama `main` se presente. Restituisce 0 se ok. */
int interp_run(AstNode *program);

#endif
