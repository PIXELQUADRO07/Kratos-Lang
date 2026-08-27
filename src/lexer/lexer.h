#ifndef KRATOS_LEXER_H
#define KRATOS_LEXER_H

#include <stddef.h>

/*
 * Tipi di token riconosciuti dal lexer.
 */
typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,

    /* Identificatori e keyword */
    TOKEN_IDENTIFIER,

    TOKEN_K_INT,
    TOKEN_K_FLOAT,
    TOKEN_K_BOOL,
    TOKEN_K_CHAR,
    TOKEN_K_STRING,
    TOKEN_K_CONST,
    TOKEN_K_VOID,

    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,

    TOKEN_HOLD,
    TOKEN_PRESS,
    TOKEN_DRIVE,
    TOKEN_SWEEP,
    TOKEN_SNAP,
    TOKEN_PUSH,
    TOKEN_IN,

    TOKEN_CRAFT,
    TOKEN_YIELD,
    TOKEN_SHOUT,
    TOKEN_WIELD,

    /* Valori */
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,
    TOKEN_TRUE,
    TOKEN_FALSE,

    /* Operatori */
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,

    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,

    /* Simboli */
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,

    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_DOT
} TokenType;


/*
 * Un token prodotto dal lexer.
 */
typedef struct {
    TokenType type;

    /*
     * Puntatore al testo originale.
     * Non facciamo ancora una copia della stringa.
     */
    const char *start;

    /*
     * Lunghezza del token.
     */
    size_t length;

    /*
     * Numero di riga nel file sorgente.
     */
    size_t line;
} Token;


/*
 * Stato corrente del lexer.
 */
typedef struct {
    const char *source;
    size_t position;
    size_t line;
} Lexer;


/*
 * Inizializza il lexer.
 */
void lexer_init(Lexer *lexer, const char *source);


/*
 * Restituisce il prossimo token.
 */
Token lexer_next_token(Lexer *lexer);


/*
 * Converte un TokenType nel suo nome.
 */
const char *token_type_name(TokenType type);

#endif
