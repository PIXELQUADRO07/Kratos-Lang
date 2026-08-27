#include "lexer.h"

#include <ctype.h>
#include <string.h>


static char current_char(Lexer *lexer)
{
    return lexer->source[lexer->position];
}


static void advance(Lexer *lexer)
{
    if (current_char(lexer) != '\0') {
        lexer->position++;
    }
}


static void skip_whitespace(Lexer *lexer)
{
    while (1) {

        char c = current_char(lexer);

        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lexer);
        }
        else if (c == '\n') {
            lexer->line++;
            advance(lexer);
        }
        else {
            break;
        }
    }
}


static TokenType identifier_type(const char *start, size_t length)
{
    if (length == 5 && strncmp(start, "k_int", 5) == 0)
        return TOKEN_K_INT;

    if (length == 7 && strncmp(start, "k_float", 7) == 0)
        return TOKEN_K_FLOAT;

    if (length == 6 && strncmp(start, "k_bool", 6) == 0)
        return TOKEN_K_BOOL;

    if (length == 6 && strncmp(start, "k_char", 6) == 0)
        return TOKEN_K_CHAR;

    if (length == 8 && strncmp(start, "k_string", 8) == 0)
        return TOKEN_K_STRING;

    if (length == 7 && strncmp(start, "k_const", 7) == 0)
        return TOKEN_K_CONST;

    if (length == 2 && strncmp(start, "if", 2) == 0)
        return TOKEN_IF;

    if (length == 4 && strncmp(start, "elif", 4) == 0)
        return TOKEN_ELIF;

    if (length == 4 && strncmp(start, "else", 4) == 0)
        return TOKEN_ELSE;

    if (length == 4 && strncmp(start, "hold", 4) == 0)
        return TOKEN_HOLD;

    if (length == 5 && strncmp(start, "press", 5) == 0)
        return TOKEN_PRESS;

    if (length == 5 && strncmp(start, "drive", 5) == 0)
        return TOKEN_DRIVE;

    if (length == 5 && strncmp(start, "sweep", 5) == 0)
        return TOKEN_SWEEP;

    if (length == 4 && strncmp(start, "snap", 4) == 0)
        return TOKEN_SNAP;

    if (length == 4 && strncmp(start, "push", 4) == 0)
        return TOKEN_PUSH;

    if (length == 5 && strncmp(start, "craft", 5) == 0)
        return TOKEN_CRAFT;

    if (length == 5 && strncmp(start, "yield", 5) == 0)
        return TOKEN_YIELD;

    if (length == 5 && strncmp(start, "shout", 5) == 0)
        return TOKEN_SHOUT;

    if (length == 5 && strncmp(start, "wield", 5) == 0)
        return TOKEN_WIELD;

    if (length == 4 && strncmp(start, "true", 4) == 0)
        return TOKEN_TRUE;

    if (length == 5 && strncmp(start, "false", 5) == 0)
        return TOKEN_FALSE;

    return TOKEN_IDENTIFIER;
}


void lexer_init(Lexer *lexer, const char *source)
{
    lexer->source = source;
    lexer->position = 0;
    lexer->line = 1;
}

static Token lexer_number(Lexer *lexer)
{
    size_t start = lexer->position;

    while (isdigit((unsigned char)current_char(lexer))) {
        advance(lexer);
    }

    TokenType type = TOKEN_INTEGER;

    /*
     * Se dopo le cifre troviamo un punto
     * seguito da almeno una cifra,
     * abbiamo un numero decimale.
     */
    if (current_char(lexer) == '.' &&
        isdigit((unsigned char)lexer->source[lexer->position + 1])) {

        type = TOKEN_FLOAT;

        advance(lexer);

        while (isdigit((unsigned char)current_char(lexer))) {
            advance(lexer);
        }
    }

    return (Token) {
        type,
        lexer->source + start,
        lexer->position - start,
        lexer->line
    };
}


static Token lexer_string(Lexer *lexer)
{
    size_t start = lexer->position;

    /*
     * Saltiamo il primo ".
     */
    advance(lexer);

    while (current_char(lexer) != '"' &&
           current_char(lexer) != '\0') {

        if (current_char(lexer) == '\n') {
            lexer->line++;
        }

        advance(lexer);
    }

    /*
     * Saltiamo il " finale.
     */
    if (current_char(lexer) == '"') {
        advance(lexer);
    }

    return (Token){
        TOKEN_STRING_LITERAL,
        lexer->source + start,
        lexer->position - start,
        lexer->line
    };
}


static Token lexer_char(Lexer *lexer)
{
    size_t start = lexer->position;

    /*
     * Saltiamo il primo '.
     */
    advance(lexer);

    if (current_char(lexer) != '\0' &&
        current_char(lexer) != '\n') {

        advance(lexer);
    }

    /*
     * Saltiamo il ' finale.
     */
    if (current_char(lexer) == '\'') {
        advance(lexer);
    }

    return (Token){
        TOKEN_CHAR_LITERAL,
        lexer->source + start,
        lexer->position - start,
        lexer->line
    };
}


Token lexer_next_token(Lexer *lexer)
{
    skip_whitespace(lexer);

    char c = current_char(lexer);

    /*
     * Fine del file.
     */
    if (c == '\0') {
        return (Token){
            TOKEN_EOF,
            lexer->source + lexer->position,
            0,
            lexer->line
        };
    }


    /*
     * Identificatori e keyword.
     *
     * Esempio:
     *
     * k_int
     * Numero
     * craft
     * hello
     */
    if (isalpha((unsigned char)c) || c == '_') {

        size_t start = lexer->position;

        while (isalnum((unsigned char)current_char(lexer)) ||
               current_char(lexer) == '_') {

            advance(lexer);
        }

        size_t length = lexer->position - start;

        return (Token){
            identifier_type(
                lexer->source + start,
                length
            ),

            lexer->source + start,
            length,
            lexer->line
        };
    }


    /*
     * Per ora riconosciamo solo '='.
     */
    if (c == '=') {

        size_t start = lexer->position;

        advance(lexer);

        return (Token){
            TOKEN_ASSIGN,
            lexer->source + start,
            1,
            lexer->line
        };
    }


    /*
     * Punto e virgola.
     */
    if (c == ';') {

        size_t start = lexer->position;

        advance(lexer);

        return (Token){
            TOKEN_SEMICOLON,
            lexer->source + start,
            1,
            lexer->line
        };
    }


    if (isdigit((unsigned char)c)) {
        return lexer_number(lexer);
    }


    if (c == '"') {
        return lexer_string(lexer);
    }


    if (c == '\'') {
        return lexer_char(lexer);
    }


    /*
     * Carattere non riconosciuto.
     *
     * Lo consumiamo e restituiamo TOKEN_ERROR.
     */
    Token token = {
        TOKEN_ERROR,
        lexer->source + lexer->position,
        1,
        lexer->line
    };

    advance(lexer);

    return token;
}


const char *token_type_name(TokenType type)
{
    switch (type) {

        case TOKEN_EOF:
            return "EOF";

        case TOKEN_ERROR:
            return "ERROR";

        case TOKEN_IDENTIFIER:
            return "IDENTIFIER";

        case TOKEN_K_INT:
            return "K_INT";

        case TOKEN_K_FLOAT:
            return "K_FLOAT";

        case TOKEN_K_BOOL:
            return "K_BOOL";

        case TOKEN_K_CHAR:
            return "K_CHAR";

        case TOKEN_K_STRING:
            return "K_STRING";

        case TOKEN_K_CONST:
            return "K_CONST";

        case TOKEN_IF:
            return "IF";

        case TOKEN_ELIF:
            return "ELIF";

        case TOKEN_ELSE:
            return "ELSE";

        case TOKEN_HOLD:
            return "HOLD";

        case TOKEN_PRESS:
            return "PRESS";

        case TOKEN_DRIVE:
            return "DRIVE";

        case TOKEN_SWEEP:
            return "SWEEP";

        case TOKEN_SNAP:
            return "SNAP";

        case TOKEN_PUSH:
            return "PUSH";

        case TOKEN_CRAFT:
            return "CRAFT";

        case TOKEN_YIELD:
            return "YIELD";

        case TOKEN_SHOUT:
            return "SHOUT";

        case TOKEN_WIELD:
            return "WIELD";

        case TOKEN_INTEGER:
            return "INTEGER";

        case TOKEN_FLOAT:
            return "FLOAT";

        case TOKEN_STRING_LITERAL:
            return "STRING";

        case TOKEN_CHAR_LITERAL:
            return "CHAR";

        case TOKEN_TRUE:
            return "TRUE";

        case TOKEN_FALSE:
            return "FALSE";

        case TOKEN_ASSIGN:
            return "ASSIGN";

        case TOKEN_PLUS:
            return "PLUS";

        case TOKEN_MINUS:
            return "MINUS";

        case TOKEN_STAR:
            return "STAR";

        case TOKEN_SLASH:
            return "SLASH";

        case TOKEN_PERCENT:
            return "PERCENT";

        case TOKEN_EQUAL:
            return "EQUAL";

        case TOKEN_NOT_EQUAL:
            return "NOT_EQUAL";

        case TOKEN_LESS:
            return "LESS";

        case TOKEN_GREATER:
            return "GREATER";

        case TOKEN_LESS_EQUAL:
            return "LESS_EQUAL";

        case TOKEN_GREATER_EQUAL:
            return "GREATER_EQUAL";

        case TOKEN_AND:
            return "AND";

        case TOKEN_OR:
            return "OR";

        case TOKEN_LPAREN:
            return "LPAREN";

        case TOKEN_RPAREN:
            return "RPAREN";

        case TOKEN_LBRACE:
            return "LBRACE";

        case TOKEN_RBRACE:
            return "RBRACE";

        case TOKEN_LBRACKET:
            return "LBRACKET";

        case TOKEN_RBRACKET:
            return "RBRACKET";

        case TOKEN_SEMICOLON:
            return "SEMICOLON";

        case TOKEN_COMMA:
            return "COMMA";

        case TOKEN_DOT:
            return "DOT";
    }

    return "UNKNOWN";
}