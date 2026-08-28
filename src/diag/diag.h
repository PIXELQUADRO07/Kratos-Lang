#ifndef KRATOS_DIAG_H
#define KRATOS_DIAG_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING
} DiagSeverity;

typedef struct {
    const char *filename;
    const char *source;
} DiagContext;

typedef struct {
    DiagSeverity severity;
    int code;
    const char *message;
    const char *filename;
    size_t line;
    size_t column;
    size_t length;
    const char *help;
} Diagnostic;

/* Lexer */
#define DIAG_K101 101 /* unterminated $ comment */
#define DIAG_K102 102 /* unterminated string */
#define DIAG_K103 103 /* invalid or unterminated character literal */
#define DIAG_K104 104 /* invalid escape sequence */
#define DIAG_K105 105 /* unexpected character */

/* Parser */
#define DIAG_K201 201 /* invalid token */
#define DIAG_K202 202 /* unexpected token / expected something else */
#define DIAG_K203 203 /* expected a type */
#define DIAG_K204 204 /* expected an expression */

/* Semantic */
#define DIAG_K301 301 /* duplicate name in scope */
#define DIAG_K302 302 /* undeclared name */
#define DIAG_K303 303 /* craft used as a value */
#define DIAG_K304 304 /* type mismatch */
#define DIAG_K305 305 /* condition is not k_bool */
#define DIAG_K306 306 /* snap/push outside a loop */
#define DIAG_K307 307 /* invalid yield */
#define DIAG_K308 308 /* wield error */
#define DIAG_K309 309 /* assignment to k_const */
#define DIAG_K310 310 /* missing yield on a path */
#define DIAG_K311 311 /* k_void used as a variable or parameter */
#define DIAG_K312 312 /* invalid construct at this level */
#define DIAG_K313 313 /* nested arrays */
#define DIAG_K314 314 /* bad call */

void diag_context_init(DiagContext *ctx, const char *filename, const char *source);

void diag_set_stream(FILE *out);

void diag_emit(const DiagContext *ctx, const Diagnostic *diagnostic);

void diag_emitf(
    const DiagContext *ctx,
    DiagSeverity severity,
    int code,
    size_t line,
    size_t column,
    size_t length,
    const char *help,
    const char *fmt,
    ...
);

#endif
