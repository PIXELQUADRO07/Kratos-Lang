#include "ast/ast.h"
#include "diag/diag.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/interp.h"
#include "semantic/semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_test_file(const char *path, const char *source)
{
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }
    fputs(source, file);
    fclose(file);
    return 1;
}

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

static int diagnostics_contain(const char *source, int semantic, const char *needle)
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
        return strstr(output, "error[K") != NULL &&
            strstr(output, "--> test.kratos:") != NULL &&
            strstr(output, needle) != NULL;
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

    collect_tokens(".5 10. 1e3 1_000", types, 32, &count);
    expect_true(types[0] == TOKEN_FLOAT, "lexer leading decimal point");
    expect_true(types[1] == TOKEN_FLOAT, "lexer trailing decimal point");
    expect_true(types[2] == TOKEN_FLOAT, "lexer exponent");
    expect_true(types[3] == TOKEN_INTEGER, "lexer digit separator");

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
    expect_true(diagnostics_contain("craft Test\n", 0, "error[K"), "parser diagnostic rendering");

    expect_true(
        diagnostics_contain("k_void craft main() { shout(Missing); }\n", 1, "undeclared identifier"),
        "semantic diagnostic rendering"
    );

    expect_true(
        diagnostics_contain(
            "k_int[] values = [1];\n"
            "k_void craft main() { values[0] = true; }\n",
            1,
            "type mismatch"
        ),
        "semantic indexed assignment diagnostic"
    );
    expect_true(
        diagnostics_contain(
            "k_void craft main() { if (1) { shout(1); } }\n",
            1,
            "condition must be k_bool"
        ),
        "semantic condition diagnostic"
    );
    expect_true(
        diagnostics_contain(
            "k_const k_int value = 1;\n"
            "k_void craft main() { value = 2; }\n",
            1,
            "cannot assign to k_const"
        ),
        "semantic const diagnostic"
    );
    expect_true(
        diagnostics_contain(
            "k_void craft main() { missing(1); }\n",
            1,
            "invalid function call"
        ),
        "semantic call diagnostic"
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

    AstNode *negative_index = parse_source(
        "k_int[] values = [1];\n"
        "k_void craft main() { shout(values[-1]); }\n",
        &had_error
    );
    expect_true(!had_error, "parser negative array index");
    expect_true(semantic_analyze(negative_index, NULL) == 0, "semantic negative array index");
    expect_true(interp_run(negative_index) != 0, "interp rejects negative array index");
    ast_free(negative_index);

    AstNode *past_end_index = parse_source(
        "k_int[] values = [1];\n"
        "k_void craft main() { shout(values[len(values)]); }\n",
        &had_error
    );
    expect_true(!had_error, "parser out of bounds array index");
    expect_true(semantic_analyze(past_end_index, NULL) == 0, "semantic out of bounds array index");
    expect_true(interp_run(past_end_index) != 0, "interp rejects out of bounds array index");
    ast_free(past_end_index);

    AstNode *const_array = parse_source(
        "k_const k_int[] values = [1];\n"
        "k_void craft main() { values[0] = 9; }\n",
        &had_error
    );
    expect_true(!had_error, "parser const array assignment");
    expect_true(semantic_analyze(const_array, NULL) != 0, "semantic rejects const array assignment");
    ast_free(const_array);

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

    AstNode *empty_array = parse_source(
        "k_int[] empty = [];\n"
        "k_void craft main() { shout(len(empty)); }\n",
        &had_error
    );
    expect_true(!had_error, "parser empty array length");
    expect_true(semantic_analyze(empty_array, NULL) == 0, "semantic empty array length");
    expect_true(interp_run(empty_array) == 0, "interp empty array length");
    ast_free(empty_array);

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

    AstNode *division_by_zero = parse_source(
        "k_void craft main() { shout(4 / 0); }\n",
        &had_error
    );
    expect_true(!had_error, "parser division by zero");
    expect_true(semantic_analyze(division_by_zero, NULL) == 0, "semantic division by zero");
    expect_true(interp_run(division_by_zero) != 0, "interp rejects division by zero");
    ast_free(division_by_zero);

    const char *cycle_a_path = "/tmp/kratos_cycle_a.kratos";
    const char *cycle_b_path = "/tmp/kratos_cycle_b.kratos";
    const char *cycle_main_path = "/tmp/kratos_cycle_main.kratos";
    expect_true(
        write_test_file(cycle_a_path, "wield \"kratos_cycle_b.kratos\";\n") &&
        write_test_file(cycle_b_path, "wield \"kratos_cycle_a.kratos\";\n") &&
        write_test_file(cycle_main_path, "wield \"kratos_cycle_a.kratos\";\n"),
        "write cyclic import fixtures"
    );
    AstNode *cyclic_import = parse_source("wield \"kratos_cycle_a.kratos\";\n", &had_error);
    expect_true(!had_error, "parser cyclic import");
    expect_true(semantic_analyze(cyclic_import, cycle_main_path) != 0, "semantic rejects cyclic import");
    ast_free(cyclic_import);
    remove(cycle_a_path);
    remove(cycle_b_path);
    remove(cycle_main_path);

    AstNode *empty_strings = parse_source(
        "k_void craft main() { shout(\"\" + \"x\"); shout(\"x\" + \"\"); }\n",
        &had_error
    );
    expect_true(!had_error, "parser empty string concatenation");
    expect_true(semantic_analyze(empty_strings, NULL) == 0, "semantic empty string concatenation");
    expect_true(interp_run(empty_strings) == 0, "interp empty string concatenation");
    ast_free(empty_strings);

    AstNode *numbers = parse_source(
        "k_float a = .5;\n"
        "k_float b = 10.;\n"
        "k_float c = 1e3;\n"
        "k_int d = 1_000;\n"
        "k_void craft main() { shout(a); shout(b); shout(c); shout(d); }\n",
        &had_error
    );
    expect_true(!had_error, "parser extended numeric literals");
    expect_true(semantic_analyze(numbers, NULL) == 0, "semantic extended numeric literals");
    expect_true(interp_run(numbers) == 0, "interp extended numeric literals");
    ast_free(numbers);

    AstNode *nested = parse_source(
        "k_int[][] matrix = [[1, 2], [3, 4]];\n"
        "k_void craft main() { matrix[1][0] = 8; shout(matrix[1][0]); }\n",
        &had_error
    );
    expect_true(!had_error, "parser nested arrays");
    expect_true(semantic_analyze(nested, NULL) == 0, "semantic nested arrays");
    expect_true(interp_run(nested) == 0, "interp nested arrays");
    ast_free(nested);

    AstNode *sliced = parse_source(
        "k_string text = \"Kratos\";\n"
        "k_void craft main() { shout(slice(text, 1, 4)); }\n",
        &had_error
    );
    expect_true(!had_error, "parser string slice");
    expect_true(semantic_analyze(sliced, NULL) == 0, "semantic string slice");
    expect_true(interp_run(sliced) == 0, "interp string slice");
    ast_free(sliced);

    AstNode *array_slice = parse_source(
        "k_int[] values = [1, 2, 3, 4];\n"
        "k_void craft main() { shout(slice(values, 1, 3)); }\n",
        &had_error
    );
    expect_true(!had_error, "parser array slice");
    expect_true(semantic_analyze(array_slice, NULL) == 0, "semantic array slice");
    expect_true(interp_run(array_slice) == 0, "interp array slice");
    ast_free(array_slice);

    AstNode *converted = parse_source(
        "k_void craft main() { shout(to_string(42)); shout(to_int(3.9)); shout(to_float(2)); }\n",
        &had_error
    );
    expect_true(!had_error, "parser scalar conversions");
    expect_true(semantic_analyze(converted, NULL) == 0, "semantic scalar conversions");
    expect_true(interp_run(converted) == 0, "interp scalar conversions");
    ast_free(converted);

    AstNode *converted_text = parse_source(
        "k_void craft main() { shout(to_int(\"42\")); shout(to_float(\"3.5\")); }\n",
        &had_error
    );
    expect_true(!had_error, "parser string conversions");
    expect_true(semantic_analyze(converted_text, NULL) == 0, "semantic string conversions");
    expect_true(interp_run(converted_text) == 0, "interp string conversions");
    ast_free(converted_text);

    /* --- Record / Struct Tests --- */

    /* 1. Basic record declaration, literal, and member access */
    AstNode *rec_basic = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point origin = Point { x: 10, y: 20 };\n"
        "k_void craft main() { shout(origin.x); shout(origin.y); }\n",
        &had_error
    );
    expect_true(!had_error, "parser basic record");
    expect_true(semantic_analyze(rec_basic, NULL) == 0, "semantic basic record");
    expect_true(interp_run(rec_basic) == 0, "interp basic record");
    ast_free(rec_basic);

    /* 2. Member mutation */
    AstNode *rec_mutation = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point p = Point { x: 1, y: 2 };\n"
        "k_void craft main() { p.x = 99; shout(p.x); shout(p.y); }\n",
        &had_error
    );
    expect_true(!had_error, "parser record mutation");
    expect_true(semantic_analyze(rec_mutation, NULL) == 0, "semantic record mutation");
    expect_true(interp_run(rec_mutation) == 0, "interp record mutation");
    ast_free(rec_mutation);

    /* 3. Record as parameter and return value (value semantics / copy) */
    AstNode *rec_func = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point craft move_point(Point pt, k_int dx) {\n"
        "    pt.x = pt.x + dx;\n"
        "    yield pt;\n"
        "}\n"
        "k_void craft main() {\n"
        "    Point a = Point { x: 5, y: 10 };\n"
        "    Point b = move_point(a, 3);\n"
        "    shout(a.x);\n"
        "    shout(b.x);\n"
        "}\n",
        &had_error
    );
    expect_true(!had_error, "parser record function");
    expect_true(semantic_analyze(rec_func, NULL) == 0, "semantic record function");
    expect_true(interp_run(rec_func) == 0, "interp record function");
    ast_free(rec_func);

    /* 4. Array of records */
    AstNode *rec_array = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point[] pts = [Point { x: 1, y: 2 }, Point { x: 3, y: 4 }];\n"
        "k_void craft main() {\n"
        "    shout(pts[0].x);\n"
        "    shout(pts[1].y);\n"
        "}\n",
        &had_error
    );
    expect_true(!had_error, "parser array of records");
    expect_true(semantic_analyze(rec_array, NULL) == 0, "semantic array of records");
    expect_true(interp_run(rec_array) == 0, "interp array of records");
    ast_free(rec_array);

    /* 5. Nested records */
    AstNode *rec_nested = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "record Line { Point p1; Point p2; }\n"
        "Line l = Line { p1: Point { x: 1, y: 2 }, p2: Point { x: 10, y: 20 } };\n"
        "k_void craft main() {\n"
        "    shout(l.p1.x);\n"
        "    shout(l.p2.y);\n"
        "}\n",
        &had_error
    );
    expect_true(!had_error, "parser nested records");
    expect_true(semantic_analyze(rec_nested, NULL) == 0, "semantic nested records");
    expect_true(interp_run(rec_nested) == 0, "interp nested records");
    ast_free(rec_nested);

    /* 6. Semantic error: unknown field in literal */
    AstNode *rec_err1 = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point p = Point { x: 1, z: 2 };\n"
        "k_void craft main() { shout(p.x); }\n",
        &had_error
    );
    expect_true(!had_error, "parser unknown field in literal");
    expect_true(semantic_analyze(rec_err1, NULL) != 0, "semantic rejects unknown field in literal");
    ast_free(rec_err1);

    /* 7. Semantic error: missing field in literal */
    AstNode *rec_err2 = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point p = Point { x: 1 };\n"
        "k_void craft main() { shout(p.x); }\n",
        &had_error
    );
    expect_true(!had_error, "parser missing field in literal");
    expect_true(semantic_analyze(rec_err2, NULL) != 0, "semantic rejects missing field in literal");
    ast_free(rec_err2);

    /* 8. Semantic error: duplicate field in literal */
    AstNode *rec_err3 = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point p = Point { x: 1, x: 2, y: 3 };\n"
        "k_void craft main() { shout(p.x); }\n",
        &had_error
    );
    expect_true(!had_error, "parser duplicate field in literal");
    expect_true(semantic_analyze(rec_err3, NULL) != 0, "semantic rejects duplicate field in literal");
    ast_free(rec_err3);

    /* 9. Semantic error: field type mismatch */
    AstNode *rec_err4 = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point p = Point { x: \"text\", y: 3 };\n"
        "k_void craft main() { shout(p.x); }\n",
        &had_error
    );
    expect_true(!had_error, "parser field type mismatch");
    expect_true(semantic_analyze(rec_err4, NULL) != 0, "semantic rejects field type mismatch");
    ast_free(rec_err4);

    /* 10. Semantic error: access unknown member */
    AstNode *rec_err5 = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "Point p = Point { x: 1, y: 2 };\n"
        "k_void craft main() { shout(p.z); }\n",
        &had_error
    );
    expect_true(!had_error, "parser access unknown member");
    expect_true(semantic_analyze(rec_err5, NULL) != 0, "semantic rejects access unknown member");
    ast_free(rec_err5);

    /* 11. Semantic error: assign to member of k_const record */
    AstNode *rec_err6 = parse_source(
        "record Point { k_int x; k_int y; }\n"
        "k_const Point p = Point { x: 1, y: 2 };\n"
        "k_void craft main() { p.x = 5; }\n",
        &had_error
    );
    expect_true(!had_error, "parser assign to member of const record");
    expect_true(semantic_analyze(rec_err6, NULL) != 0, "semantic rejects mutation of const record member");
    ast_free(rec_err6);

    if (g_failed != 0) {
        fprintf(stderr, "%d failed, %d passed\n", g_failed, g_passed);
        return 1;
    }

    printf("%d tests passed\n", g_passed);
    return 0;
}
