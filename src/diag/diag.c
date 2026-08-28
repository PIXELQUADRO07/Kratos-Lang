#include "diag/diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static FILE *g_diag_stream = NULL;

void diag_context_init(DiagContext *ctx, const char *filename, const char *source)
{
    ctx->filename = (filename != NULL && filename[0] != '\0') ? filename : "<stdin>";
    ctx->source = source;
}

void diag_set_stream(FILE *out)
{
    g_diag_stream = out;
}

static FILE *diag_stream(void)
{
    return g_diag_stream != NULL ? g_diag_stream : stderr;
}

static const char *line_text(const char *source, size_t line, size_t *out_len)
{
    if (source == NULL || line == 0) {
        return NULL;
    }

    size_t current = 1;
    const char *p = source;
    while (*p != '\0' && current < line) {
        if (*p == '\n') {
            current++;
        }
        p++;
    }
    if (current != line) {
        return NULL;
    }

    const char *start = p;
    while (*p != '\0' && *p != '\n' && *p != '\r') {
        p++;
    }
    *out_len = (size_t)(p - start);
    return start;
}

void diag_emit(const DiagContext *ctx, const Diagnostic *diagnostic)
{
    FILE *out = diag_stream();
    const char *filename = diagnostic->filename;
    if (filename == NULL) {
        filename = (ctx != NULL && ctx->filename != NULL) ? ctx->filename : "<stdin>";
    }
    const char *source = (ctx != NULL) ? ctx->source : NULL;
    const char *kind = diagnostic->severity == DIAG_WARNING ? "warning" : "error";
    size_t column = diagnostic->column == 0 ? 1 : diagnostic->column;
    size_t length = diagnostic->length == 0 ? 1 : diagnostic->length;

    fprintf(out, "%s[K%03d]: %s\n", kind, diagnostic->code, diagnostic->message);
    fprintf(out, "  --> %s:%zu:%zu\n", filename, diagnostic->line, column);

    size_t src_len = 0;
    const char *src_line = line_text(source, diagnostic->line, &src_len);
    if (src_line != NULL) {
        fprintf(out, "   |\n");
        fprintf(out, "%4zu | %.*s\n", diagnostic->line, (int)src_len, src_line);
        fprintf(out, "   | ");
        size_t pad = column > 0 ? column - 1 : 0;
        if (pad > src_len) {
            pad = src_len;
        }
        for (size_t i = 0; i < pad; i++) {
            fputc(src_line[i] == '\t' ? '\t' : ' ', out);
        }
        size_t marks = length;
        if (pad + marks > src_len && src_len >= pad) {
            marks = src_len - pad;
        }
        if (marks == 0) {
            marks = 1;
        }
        for (size_t i = 0; i < marks; i++) {
            fputc('^', out);
        }
        fputc('\n', out);
    }

    if (diagnostic->help != NULL && diagnostic->help[0] != '\0') {
        fprintf(out, "   |\n");
        fprintf(out, "   = help: %s\n", diagnostic->help);
    }
    fputc('\n', out);
}

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
)
{
    char message[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    Diagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.filename = ctx != NULL ? ctx->filename : NULL;
    diagnostic.line = line;
    diagnostic.column = column;
    diagnostic.length = length;
    diagnostic.help = help;
    diag_emit(ctx, &diagnostic);
}
