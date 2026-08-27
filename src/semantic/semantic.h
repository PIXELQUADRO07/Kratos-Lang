#ifndef KRATOS_SEMANTIC_H
#define KRATOS_SEMANTIC_H

#include "ast/ast.h"

/*
 * Espande i `wield`, costruisce la tabella dei simboli e controlla i tipi.
 * `source_path` puo' essere NULL (stdin): i percorsi di wield sono allora
 * relativi alla directory corrente.
 *
 * Restituisce 0 se l'analisi ha successo. `program` puo' essere modificato
 * (i nodi wield vengono sostituiti dalle dichiarazioni importate).
 */
int semantic_analyze(AstNode *program, const char *source_path);

#endif
