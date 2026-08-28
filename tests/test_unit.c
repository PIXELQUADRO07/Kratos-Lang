#include "ast/ast.h"
#include "diag/diag.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/interp.h"
#include "semantic/semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failed = 0;
static int g_passed = 0;

static void expect_true(int cond, const char *name)
{
    if (cond) {
        g_passed++;
    } else {
        g_failed++;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}


static void collect_tokens(const char *source, TokenType *out, size_t max, size_t *count)
{
    Lexer lexer;
    lexer_init(&lexer, source);
    *count = 0;
    while (*count < max) {
        Token token = lexer_next_token(&lexer);
        out[*count] = token.type;
        *count += 1;
        if (token.type == TOKEN_EOF) {
            break;
        }
    }
}


static AstNode *parse_source(const char *source, int *had_error)
{
    Lexer lexer;
    Parser parser;
    lexer_init(&lexer, source);
    parser_init(&parser, &lexer);
    AstNode *program = parser_parse_program(&parser);
    *had_error = parser.had_error;
    return program;
}

static int diagnostics_contain(const char *source, int semantic)
{
    char output[4096];
    FILE *stream = tmpfile();
    if (stream == NULL) {
        return 0;
    }

    DiagContext context;
    diag_context_init(&context, "test.kratos", source);
    diag_set_stream(stream);

    Lexer lexer;
    Parser parser;
    lexer_init(&lexer, source);
    parser_init_with_context(&parser, &lexer, &context);
    AstNode *program = parser_parse_program(&parser);
    if (semantic) {
        semantic_analyze_with_context(program, NULL, &context);
    }
    ast_free(program);

    fflush(stream);
    rewind(stream);
    size_t length = fread(output, 1, sizeof(output) - 1, stream);
    output[length] = '\0';
    fclose(stream);
    diag_set_stream(NULL);
    return strstr(output, "error[K") != NULL && strstr(output, "--> test.kratos:") != NULL;
}


int main(void)
{
    TokenType types[32];
    size_t count = 0;

    collect_tokens("k_int x = 1;", types, 32, &count);
    expect_true(count >= 5, "lexer declaration token count");
    expect_true(types[0] == TOKEN_K_INT, "lexer k_int");
    expect_true(types[1] == TOKEN_IDENTIFIER, "lexer identifier");
    expect_true(types[2] == TOKEN_ASSIGN, "lexer assign");
    expect_true(types[3] == TOKEN_INTEGER, "lexer integer");
    expect_true(types[4] == TOKEN_SEMICOLON, "lexer semicolon");

    collect_tokens("a == b", types, 32, &count);
    expect_true(types[1] == TOKEN_EQUAL, "lexer ==");

    collect_tokens("a = b", types, 32, &count);
    expect_true(types[1] == TOKEN_ASSIGN, "lexer =");

    collect_tokens("craftwork", types, 32, &count);
    expect_true(types[0] == TOKEN_IDENTIFIER, "lexer craftwork is identifier");

    collect_tokens("$ comment $\nk_bool t = true;", types, 32, &count);
    expect_true(types[0] == TOKEN_K_BOOL, "lexer skips comments");

    collect_tokens("\"hi\\n\"", types, 32, &count);
    expect_true(types[0] == TOKEN_STRING_LITERAL, "lexer string with escape");

    collect_tokens("\"unterminated", types, 32, &count);
    expect_true(types[0] == TOKEN_ERROR, "lexer unterminated string is error");

    int had_error = 0;
    AstNode *ok = parse_source(
        "k_void craft main() { shout(\"x\"); }\n",
        &had_error
    );
    expect_true(!had_error, "parser valid craft");
    expect_true(ok->as.program.declarations.count == 1, "parser one top-level decl");
    expect_true(ok->as.program.declarations.items[0]->kind == AST_FUNC_DECL, "parser func decl");
    ast_free(ok);

    AstNode *bad = parse_source("craft Test\n", &had_error);
    expect_true(had_error, "parser rejects bare craft");
    ast_free(bad);
    expect_true(diagnostics_contain("craft Test\n", 0), "parser diagnostic rendering");

    expect_true(
        diagnostics_contain("k_void craft main() { shout(Missing); }\n", 1),
        "semantic diagnostic rendering"
    );

    AstNode *void_var = parse_source("k_void x = 1;\n", &had_error);
    expect_true(!had_error, "parser accepts k_void var syntactically");
    expect_true(semantic_analyze(void_var, NULL) != 0, "semantic rejects k_void variable");
    ast_free(void_var);

    AstNode *const_assign = parse_source(
        "k_const k_int x = 1;\nk_void craft main() { x = 2; }\n",
        &had_error
    );
    expect_true(!had_error, "parser const assign");
    expect_true(semantic_analyze(const_assign, NULL) != 0, "semantic rejects assign to k_const");
    ast_free(const_assign);

    AstNode *run = parse_source(
        "k_int craft add(k_int a, k_int b) { yield a + b; }\n"
        "k_void craft main() { add(1, 2); }\n",
        &had_error
    );
    expect_true(!had_error, "parser shout add");
    expect_true(semantic_analyze(run, NULL) == 0, "semantic shout add");
    expect_true(interp_run(run) == 0, "interp shout add");
    ast_free(run);

    AstNode *indexed = parse_source(
        "k_int[] values = [1, 2];\n"
        "k_void craft main() { values[0] = 7; shout(values[0]); }\n",
        &had_error
    );
    expect_true(!had_error, "parser indexed assignment");
    expect_true(semantic_analyze(indexed, NULL) == 0, "semantic indexed assignment");
    expect_true(interp_run(indexed) == 0, "interp indexed assignment");
    ast_free(indexed);

    AstNode *lengths = parse_source(
        "k_string text = \"hello\";\n"
        "k_int[] values = [1, 2, 3];\n"
        "k_void craft main() { shout(len(text)); shout(len(values)); }\n",
        &had_error
    );
    expect_true(!had_error, "parser len builtin");
    expect_true(semantic_analyze(lengths, NULL) == 0, "semantic len builtin");
    expect_true(interp_run(lengths) == 0, "interp len builtin");
    ast_free(lengths);

    AstNode *strings = parse_source(
        "k_string left = \"hello, \";\n"
        "k_string right = \"Kratos\";\n"
        "k_void craft main() { shout(left + right); }\n",
        &had_error
    );
    expect_true(!had_error, "parser string concatenation");
    expect_true(semantic_analyze(strings, NULL) == 0, "semantic string concatenation");
    expect_true(interp_run(strings) == 0, "interp string concatenation");
    ast_free(strings);

    if (g_failed != 0) {
        fprintf(stderr, "%d failed, %d passed\n", g_failed, g_passed);
        return 1;
    }

    printf("%d tests passed\n", g_passed);
    return 0;
}
