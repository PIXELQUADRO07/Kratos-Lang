#include "lexer.h"
#include "diag/diag.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static size_t column_at(const Lexer *lexer, size_t index)
{
    size_t column = 1;
    for (size_t i = 0; i < index; i++) {
        if (lexer->source[i] == '\n') {
            column = 1;
        } else {
            column++;
        }
    }
    return column;
}


static Token make_token(Lexer *lexer, TokenType type, size_t start, size_t length, size_t line)
{
    Token token;
    token.type = type;
    token.start = lexer->source + start;
    token.length = length;
    token.line = line;
    token.column = column_at(lexer, start);
    token.error_code = 0;
    return token;
}


static Token make_error_token(Lexer *lexer, size_t start, size_t length, size_t line, int error_code)
{
    Token token = make_token(lexer, TOKEN_ERROR, start, length, line);
    token.error_code = error_code;
    return token;
}


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


/*
 * Se il carattere corrente e' 'expected', lo consuma e restituisce 1.
 * Altrimenti non consuma nulla e restituisce 0. Usata per gli operatori a
 * due caratteri (==, !=, <=, >=, &&, ||).
 */
static int match_char(Lexer *lexer, char expected)
{
    if (current_char(lexer) != expected) {
        return 0;
    }
    advance(lexer);
    return 1;
}


/*
 * Commento: $ ... $. Simmetrico, senza bisogno di una forma diversa per
 * riga singola/multi-riga: il commento finisce al prossimo '$', anche se
 * nel mezzo ci sono newline (che comunque aggiornano il contatore riga).
 */
static void skip_comment(Lexer *lexer)
{
    /* Saltiamo il '$' di apertura. */
    advance(lexer);

    while (current_char(lexer) != '$' && current_char(lexer) != '\0') {
        if (current_char(lexer) == '\n') {
            lexer->line++;
        }
        advance(lexer);
    }

    if (current_char(lexer) == '$') {
        advance(lexer);
    } else {
        lexer->unterminated_comment = 1;
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
        else if (c == '$') {
            skip_comment(lexer);
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

    if (length == 6 && strncmp(start, "k_void", 6) == 0)
        return TOKEN_K_VOID;

    if (length == 3 && strncmp(start, "not", 3) == 0)
        return TOKEN_NOT;

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

    if (length == 2 && strncmp(start, "in", 2) == 0)
        return TOKEN_IN;

    if (length == 5 && strncmp(start, "craft", 5) == 0)
        return TOKEN_CRAFT;

    if (length == 5 && strncmp(start, "yield", 5) == 0)
        return TOKEN_YIELD;

    if (length == 5 && strncmp(start, "shout", 5) == 0)
        return TOKEN_SHOUT;

    if (length == 5 && strncmp(start, "wield", 5) == 0)
        return TOKEN_WIELD;

    if (length == 6 && strncmp(start, "record", 6) == 0)
        return TOKEN_RECORD;

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
    lexer->unterminated_comment = 0;
}

static Token lexer_number(Lexer *lexer)
{
    size_t start = lexer->position;

    while (isdigit((unsigned char)current_char(lexer)) ||
           (current_char(lexer) == '_' &&
            isdigit((unsigned char)lexer->source[lexer->position + 1]))) {
        advance(lexer);
    }

    TokenType type = TOKEN_INTEGER;

    /*
     * Un punto appartiene al numero anche quando non e' seguito da cifre
     * (10.); il caso .5 viene instradato qui dal lexer principale.
     */
    if (current_char(lexer) == '.') {
        type = TOKEN_FLOAT;
        advance(lexer);

        while (isdigit((unsigned char)current_char(lexer)) ||
               (current_char(lexer) == '_' &&
                isdigit((unsigned char)lexer->source[lexer->position + 1]))) {
            advance(lexer);
        }
    }

    if (current_char(lexer) == 'e' || current_char(lexer) == 'E') {
        size_t exponent = lexer->position + 1;
        if (lexer->source[exponent] == '+' || lexer->source[exponent] == '-') {
            exponent++;
        }
        if (isdigit((unsigned char)lexer->source[exponent])) {
            type = TOKEN_FLOAT;
            advance(lexer);
            if (current_char(lexer) == '+' || current_char(lexer) == '-') {
                advance(lexer);
            }
            while (isdigit((unsigned char)current_char(lexer)) ||
                   (current_char(lexer) == '_' &&
                    isdigit((unsigned char)lexer->source[lexer->position + 1]))) {
                advance(lexer);
            }
        }
    }

    return make_token(lexer, type, start, lexer->position - start, lexer->line);
}


static int is_escape_char(char c)
{
    return c == 'n' || c == 't' || c == 'r' || c == '0' ||
           c == '\\' || c == '"' || c == '\'';
}


static int consume_escape(Lexer *lexer)
{
    /* Siamo sul '\\'. */
    advance(lexer);
    char e = current_char(lexer);
    if (e == '\0' || !is_escape_char(e)) {
        return 0;
    }
    if (e == '\n') {
        lexer->line++;
    }
    advance(lexer);
    return 1;
}


static Token lexer_string(Lexer *lexer)
{
    size_t start = lexer->position;
    size_t start_line = lexer->line;

    advance(lexer);

    while (current_char(lexer) != '"' && current_char(lexer) != '\0') {
        if (current_char(lexer) == '\n') {
            lexer->line++;
        }

        if (current_char(lexer) == '\\') {
            size_t esc_start = lexer->position;
            if (!consume_escape(lexer)) {
                if (current_char(lexer) != '\0') {
                    advance(lexer);
                }
                return make_error_token(lexer, esc_start, lexer->position - esc_start, start_line, DIAG_K104);
            }
            continue;
        }

        advance(lexer);
    }

    if (current_char(lexer) != '"') {
        return make_error_token(lexer, start, lexer->position - start, start_line, DIAG_K102);
    }

    advance(lexer);
    return make_token(lexer, TOKEN_STRING_LITERAL, start, lexer->position - start, start_line);
}


static Token lexer_char(Lexer *lexer)
{
    size_t start = lexer->position;
    size_t start_line = lexer->line;

    advance(lexer);

    if (current_char(lexer) == '\\') {
        if (!consume_escape(lexer)) {
            if (current_char(lexer) != '\0') {
                advance(lexer);
            }
            return make_error_token(lexer, start, lexer->position - start, start_line, DIAG_K103);
        }
    } else if (current_char(lexer) != '\0' && current_char(lexer) != '\n') {
        advance(lexer);
    }

    if (current_char(lexer) != '\'') {
        return make_error_token(lexer, start, lexer->position - start, start_line, DIAG_K103);
    }

    advance(lexer);
    return make_token(lexer, TOKEN_CHAR_LITERAL, start, lexer->position - start, start_line);
}


Token lexer_next_token(Lexer *lexer)
{
    skip_whitespace(lexer);

    if (lexer->unterminated_comment) {
        lexer->unterminated_comment = 0;
        return make_error_token(lexer, lexer->position, 1, lexer->line, DIAG_K101);
    }

    char c = current_char(lexer);

    /*
     * Fine del file.
     */
    if (c == '\0') {
        return make_token(lexer, TOKEN_EOF, lexer->position, 0, lexer->line);
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

        return make_token(
            lexer,
            identifier_type(lexer->source + start, length),
            start,
            length,
            lexer->line
        );
    }


    if (isdigit((unsigned char)c)) {
        return lexer_number(lexer);
    }

    if (c == '.' && isdigit((unsigned char)lexer->source[lexer->position + 1])) {
        return lexer_number(lexer);
    }


    if (c == '"') {
        return lexer_string(lexer);
    }


    if (c == '\'') {
        return lexer_char(lexer);
    }


    /*
     * Operatori e simboli, in ordine dal piu' lungo al piu' corto quando
     * condividono il primo carattere (es. "==" prima di "=").
     */
    {
        size_t start = lexer->position;
        TokenType type;

        switch (c) {

            case '=':
                advance(lexer);
                type = match_char(lexer, '=') ? TOKEN_EQUAL : TOKEN_ASSIGN;
                break;

            case '!':
                advance(lexer);
                if (match_char(lexer, '=')) {
                    type = TOKEN_NOT_EQUAL;
                    break;
                }
                /* '!' da solo non e' un operatore valido: la negazione logica e' "not". */
                return make_error_token(lexer, start, 1, lexer->line, DIAG_K105);

            case '<':
                advance(lexer);
                type = match_char(lexer, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS;
                break;

            case '>':
                advance(lexer);
                type = match_char(lexer, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER;
                break;

            case '&':
                advance(lexer);
                if (match_char(lexer, '&')) {
                    type = TOKEN_AND;
                    break;
                }
                return make_error_token(lexer, start, 1, lexer->line, DIAG_K105);

            case '|':
                advance(lexer);
                if (match_char(lexer, '|')) {
                    type = TOKEN_OR;
                    break;
                }
                return make_error_token(lexer, start, 1, lexer->line, DIAG_K105);

            case '+': advance(lexer); type = TOKEN_PLUS; break;
            case '-': advance(lexer); type = TOKEN_MINUS; break;
            case '*': advance(lexer); type = TOKEN_STAR; break;
            case '/': advance(lexer); type = TOKEN_SLASH; break;
            case '%': advance(lexer); type = TOKEN_PERCENT; break;

            case '(': advance(lexer); type = TOKEN_LPAREN; break;
            case ')': advance(lexer); type = TOKEN_RPAREN; break;
            case '{': advance(lexer); type = TOKEN_LBRACE; break;
            case '}': advance(lexer); type = TOKEN_RBRACE; break;
            case '[': advance(lexer); type = TOKEN_LBRACKET; break;
            case ']': advance(lexer); type = TOKEN_RBRACKET; break;

            case ';': advance(lexer); type = TOKEN_SEMICOLON; break;
            case ',': advance(lexer); type = TOKEN_COMMA; break;
            case '.': advance(lexer); type = TOKEN_DOT; break;
            case ':': advance(lexer); type = TOKEN_COLON; break;

            default:
                /*
                 * Carattere non riconosciuto.
                 *
                 * Lo consumiamo e restituiamo TOKEN_ERROR.
                 */
                advance(lexer);
                return make_error_token(lexer, start, 1, lexer->line, DIAG_K105);
        }

        return make_token(lexer, type, start, lexer->position - start, lexer->line);
    }
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

        case TOKEN_K_VOID:
            return "K_VOID";

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

        case TOKEN_IN:
            return "IN";

        case TOKEN_CRAFT:
            return "CRAFT";

        case TOKEN_YIELD:
            return "YIELD";

        case TOKEN_SHOUT:
            return "SHOUT";

        case TOKEN_WIELD:
            return "WIELD";

        case TOKEN_RECORD:
            return "RECORD";

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

        case TOKEN_NOT:
            return "NOT";

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

        case TOKEN_COLON:
            return "COLON";
    }

    return "UNKNOWN";
}


static char decode_escape_char(char c)
{
    switch (c) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case '0':  return '\0';
        case '\\': return '\\';
        case '"':  return '"';
        case '\'': return '\'';
        default:   return c;
    }
}


char *token_decode_string(Token token)
{
    if (token.length < 2) {
        char *empty = malloc(1);
        if (empty == NULL) {
            fprintf(stderr, "kratos: memoria esaurita\n");
            exit(EXIT_FAILURE);
        }
        empty[0] = '\0';
        return empty;
    }

    const char *inner = token.start + 1;
    size_t inner_length = token.length - 2;
    char *buffer = malloc(inner_length + 1);
    if (buffer == NULL) {
        fprintf(stderr, "kratos: memoria esaurita\n");
        exit(EXIT_FAILURE);
    }

    size_t out = 0;
    for (size_t i = 0; i < inner_length; i++) {
        if (inner[i] == '\\' && i + 1 < inner_length) {
            buffer[out++] = decode_escape_char(inner[i + 1]);
            i++;
        } else {
            buffer[out++] = inner[i];
        }
    }
    buffer[out] = '\0';
    return buffer;
}


int token_decode_char(Token token, char *out)
{
    if (token.length < 3 || token.start[0] != '\'') {
        return 0;
    }

    if (token.start[1] == '\\') {
        if (token.length < 4) {
            return 0;
        }
        *out = decode_escape_char(token.start[2]);
        return 1;
    }

    *out = token.start[1];
    return 1;
}
