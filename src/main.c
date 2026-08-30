#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast/ast.h"
#include "codegen/codegen.h"
#include "diag/diag.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/interp.h"
#include "semantic/semantic.h"
#include "utils/file.h"
#include "version.h"

#include <unistd.h>

static void print_usage(FILE *out)
{
    fputs(
        "Usage: kratosc [--tokens | --ast | --check | --emit-c] [-o <path>] [file.kratos]\n"
        "       kratosc --version\n"
        "       kratosc --help\n"
        "  no flag     parse, analyze, and run the program\n"
        "  --tokens    print tokens (debug)\n"
        "  --ast       print the syntax tree (debug)\n"
        "  --check     parse and analyze without running\n"
        "  --emit-c    emit C to stdout, or to -o <path>\n",
        out
    );
}


static char *read_stdin(void)
{
    size_t capacity = 256;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        fprintf(stderr, "kratos: out of memory\n");
        exit(EXIT_FAILURE);
    }

    int c;
    while ((c = fgetc(stdin)) != EOF) {
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *grown = realloc(buffer, capacity);
            if (grown == NULL) {
                free(buffer);
                fprintf(stderr, "kratos: out of memory\n");
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
        MODE_CHECK,
        MODE_EMIT_C
    } mode = MODE_RUN;

    const char *path = NULL;
    const char *output_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("kratosc %s\n", KRATOS_VERSION);
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
        if (strcmp(argv[i], "--check") == 0) {
            mode = MODE_CHECK;
            continue;
        }
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "kratos: -o requires an output path\n");
                return 2;
            }
            output_path = argv[++i];
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "kratos: unknown flag '%s'\n", argv[i]);
            print_usage(stderr);
            return 2;
        }
        if (path != NULL) {
            fprintf(stderr, "kratos: only one input file allowed\n");
            return 2;
        }
        path = argv[i];
    }

    if (output_path != NULL && (mode == MODE_TOKENS || mode == MODE_AST || mode == MODE_CHECK)) {
        fprintf(stderr, "kratos: -o is only valid with --emit-c or without a mode flag\n");
        return 2;
    }

    char *source = NULL;
    if (path != NULL) {
        source = kratos_read_file(path);
        if (source == NULL) {
            fprintf(stderr, "kratos: cannot read '%s'\n", path);
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

    DiagContext diag_context;
    diag_context_init(&diag_context, path, source);

    Lexer lexer;
    Parser parser;
    lexer_init(&lexer, source);
    parser_init_with_context(&parser, &lexer, &diag_context);
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

    if (semantic_analyze_with_context(program, path, &diag_context) != 0) {
        ast_free(program);
        free(source);
        return 1;
    }

    int status = 0;
    if (mode == MODE_CHECK) {
        puts("Kratos-Lang: no errors found");
    } else if (mode == MODE_EMIT_C) {
        FILE *out = stdout;
        if (output_path != NULL) {
            out = fopen(output_path, "w");
            if (out == NULL) {
                fprintf(stderr, "kratos: cannot open output '%s'\n", output_path);
                ast_free(program);
                free(source);
                return 1;
            }
        }
        status = codegen_emit_c(program, out);
        if (out != stdout) {
            fclose(out);
        }
    } else if (output_path != NULL) {
        char temp_path[64];
        snprintf(temp_path, sizeof(temp_path), "/tmp/kratosc-%ld.c", (long)getpid());
        FILE *out = fopen(temp_path, "w");
        if (out == NULL) {
            fprintf(stderr, "kratos: cannot create temporary C file\n");
            status = 1;
        } else {
            status = codegen_emit_c(program, out);
            fclose(out);
            if (status == 0) {
                const char *cc = getenv("CC");
                if (cc == NULL || cc[0] == '\0') {
                    cc = "cc";
                }
                char command[1024];
                snprintf(command, sizeof(command), "%s -std=c11 '%s' -o '%s'", cc, temp_path, output_path);
                status = system(command);
            }
            remove(temp_path);
        }
    } else {
        status = interp_run(program);
    }

    ast_free(program);
    free(source);
    return status;
}
