#ifndef KRATOS_PARSER_H
#define KRATOS_PARSER_H

#include "ast/ast.h"
#include "lexer/lexer.h"

/*
 * Parser ricorsivo discendente per la grammatica in docs/specification.md.
 * Non tiene una copia della sorgente: si appoggia direttamente al Lexer,
 * chiedendo un token alla volta.
 */
typedef struct {
    Lexer *lexer;
    Token current;
    Token previous;

    /*
     * Diventa 1 alla prima syntax error incontrata. parser_parse_program
     * continua comunque a leggere il resto del file (recupero "panico
     * minimo": consuma comunque un token e prosegue) cosi' da poter
     * segnalare piu' di un errore per volta, ma l'AST risultante non va
     * considerato valido se had_error e' 1.
     */
    int had_error;
} Parser;

/*
 * Inizializza il parser e legge il primo token dal lexer.
 */
void parser_init(Parser *parser, Lexer *lexer);

/*
 * Analizza un intero programma (program = { declaration | function } ;) e
 * restituisce la radice AST_PROGRAM. Il chiamante e' proprietario del nodo
 * restituito e deve liberarlo con ast_free.
 *
 * Controllare parser->had_error dopo la chiamata: se e' 1, uno o piu' errori
 * di sintassi sono stati stampati su stderr durante l'analisi.
 */
AstNode *parser_parse_program(Parser *parser);

#endif
