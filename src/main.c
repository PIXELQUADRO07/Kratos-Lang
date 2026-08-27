#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast/ast.h"
#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/interp.h"
#include "semantic/semantic.h"
#include "utils/file.h"

static void print_usage(FILE *out)
{
    fputs(
        "uso: kratosc [--tokens | --ast | --emit-c] [file.kratos]\n"
        "  senza flag  analizza, controlla i tipi ed esegue (chiama main se presente)\n"
        "  --tokens    stampa i token\n"
        "  --ast       stampa l'albero sintattico\n"
        "  --emit-c    emette C su stdout\n"
        "  senza file  legge da stdin\n",
        out
    );
}


static char *read_stdin(void)
{
    size_t capacity = 256;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        fprintf(stderr, "kratos: memoria esaurita\n");
        exit(EXIT_FAILURE);
    }

    int c;
    while ((c = fgetc(stdin)) != EOF) {
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *grown = realloc(buffer, capacity);
            if (grown == NULL) {
                free(buffer);
                fprintf(stderr, "kratos: memoria esaurita\n");
                exit(EXIT_FAILURE);
            }
            buffer = grown;
        }
        buffer[length++] = (char)c;
    }
    buffer[length] = '\0';
    return buffer;
}


static void dump_tokens(const char *source)
{
    Lexer lexer;
    lexer_init(&lexer, source);

    Token token;
    do {
        token = lexer_next_token(&lexer);
        printf(
            "line %zu | %-15s %.*s\n",
            token.line,
            token_type_name(token.type),
            (int)token.length,
            token.start
        );
    } while (token.type != TOKEN_EOF);
}


int main(int argc, char **argv)
{
    enum {
        MODE_RUN,
        MODE_TOKENS,
        MODE_AST,
        MODE_EMIT_C
    } mode = MODE_RUN;

    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--tokens") == 0) {
            mode = MODE_TOKENS;
            continue;
        }
        if (strcmp(argv[i], "--ast") == 0) {
            mode = MODE_AST;
            continue;
        }
        if (strcmp(argv[i], "--emit-c") == 0) {
            mode = MODE_EMIT_C;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "kratos: flag sconosciuta '%s'\n", argv[i]);
            print_usage(stderr);
            return 2;
        }
        if (path != NULL) {
            fprintf(stderr, "kratos: un solo file alla volta\n");
            return 2;
        }
        path = argv[i];
    }

    char *source = NULL;
    if (path != NULL) {
        source = kratos_read_file(path);
        if (source == NULL) {
            fprintf(stderr, "kratos: impossibile leggere '%s'\n", path);
            return 1;
        }
    } else {
        source = read_stdin();
    }

    if (mode == MODE_TOKENS) {
        dump_tokens(source);
        free(source);
        return 0;
    }

    Lexer lexer;
    Parser parser;
    lexer_init(&lexer, source);
    parser_init(&parser, &lexer);
    AstNode *program = parser_parse_program(&parser);

    if (parser.had_error) {
        ast_free(program);
        free(source);
        return 1;
    }

    if (mode == MODE_AST) {
        ast_print(program, 0);
        ast_free(program);
        free(source);
        return 0;
    }

    if (semantic_analyze(program, path) != 0) {
        ast_free(program);
        free(source);
        return 1;
    }

    int status = 0;
    if (mode == MODE_EMIT_C) {
        status = codegen_emit_c(program, stdout);
    } else {
        status = interp_run(program);
    }

    ast_free(program);
    free(source);
    return status;
}
