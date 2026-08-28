#include "codegen/codegen.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *out;
    int indent;
    int temp;
    int had_error;
} Emitter;


static void emit_indent(Emitter *e)
{
    for (int i = 0; i < e->indent; i++) {
        fputs("    ", e->out);
    }
}


static void emit_c_type(FILE *out, KratosType type, int is_array)
{
    const char *base = "int64_t";
    switch (type) {
        case KRATOS_TYPE_INT:    base = "int64_t"; break;
        case KRATOS_TYPE_FLOAT:  base = "double"; break;
        case KRATOS_TYPE_BOOL:   base = "int"; break;
        case KRATOS_TYPE_CHAR:   base = "char"; break;
        case KRATOS_TYPE_STRING: base = "const char *"; break;
        case KRATOS_TYPE_VOID:   base = "void"; break;
    }

    if (is_array) {
        fprintf(out, "KArr");
        return;
    }
    fputs(base, out);
}


static void emit_string_literal(FILE *out, const char *text)
{
    fputc('"', out);
    if (text == NULL) {
        fputc('"', out);
        return;
    }
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        switch (*p) {
            case '\\': fputs("\\\\", out); break;
            case '"':  fputs("\\\"", out); break;
            case '\n': fputs("\\n", out); break;
            case '\t': fputs("\\t", out); break;
            case '\r': fputs("\\r", out); break;
            default:
                if (*p < 32) {
                    fprintf(out, "\\%03o", *p);
                } else {
                    fputc(*p, out);
                }
                break;
        }
    }
    fputc('"', out);
}


static void emit_expr(Emitter *e, const AstNode *node);


static void emit_binary_op(FILE *out, TokenType op)
{
    switch (op) {
        case TOKEN_PLUS: fputs(" + ", out); break;
        case TOKEN_MINUS: fputs(" - ", out); break;
        case TOKEN_STAR: fputs(" * ", out); break;
        case TOKEN_SLASH: fputs(" / ", out); break;
        case TOKEN_PERCENT: fputs(" % ", out); break;
        case TOKEN_EQUAL: fputs(" == ", out); break;
        case TOKEN_NOT_EQUAL: fputs(" != ", out); break;
        case TOKEN_LESS: fputs(" < ", out); break;
        case TOKEN_GREATER: fputs(" > ", out); break;
        case TOKEN_LESS_EQUAL: fputs(" <= ", out); break;
        case TOKEN_GREATER_EQUAL: fputs(" >= ", out); break;
        case TOKEN_AND: fputs(" && ", out); break;
        case TOKEN_OR: fputs(" || ", out); break;
        default: fputs(" /*op*/ ", out); break;
    }
}


static void emit_expr(Emitter *e, const AstNode *node)
{
    if (node == NULL) {
        fputs("0", e->out);
        return;
    }

    switch (node->kind) {
        case AST_LITERAL_EXPR:
            switch (node->as.literal_expr.kind) {
                case LITERAL_INT:
                    fprintf(e->out, "%lldLL", (long long)node->as.literal_expr.value.int_value);
                    break;
                case LITERAL_FLOAT:
                    fprintf(e->out, "%g", node->as.literal_expr.value.float_value);
                    break;
                case LITERAL_BOOL:
                    fputs(node->as.literal_expr.value.bool_value ? "1" : "0", e->out);
                    break;
                case LITERAL_CHAR:
                    fprintf(e->out, "'");
                    if (node->as.literal_expr.value.char_value == '\n') {
                        fputs("\\n", e->out);
                    } else if (node->as.literal_expr.value.char_value == '\\') {
                        fputs("\\\\", e->out);
                    } else if (node->as.literal_expr.value.char_value == '\'') {
                        fputs("\\'", e->out);
                    } else {
                        fputc(node->as.literal_expr.value.char_value, e->out);
                    }
                    fprintf(e->out, "'");
                    break;
                case LITERAL_STRING:
                    emit_string_literal(e->out, node->as.literal_expr.value.string_value);
                    break;
            }
            break;

        case AST_IDENTIFIER_EXPR:
            fprintf(e->out, "%s", node->as.identifier_expr.name);
            break;

        case AST_UNARY_EXPR:
            fputc('(', e->out);
            if (node->as.unary_expr.operator == TOKEN_NOT) {
                fputs("!", e->out);
            } else {
                fputs("-", e->out);
            }
            emit_expr(e, node->as.unary_expr.operand);
            fputc(')', e->out);
            break;

        case AST_BINARY_EXPR:
            fputc('(', e->out);
            emit_expr(e, node->as.binary_expr.left);
            emit_binary_op(e->out, node->as.binary_expr.operator);
            emit_expr(e, node->as.binary_expr.right);
            fputc(')', e->out);
            break;

        case AST_CALL_EXPR:
            if (strcmp(node->as.call_expr.callee, "len") == 0) {
                fputs("k_len(", e->out);
            } else {
                fprintf(e->out, "kfn_%s(", node->as.call_expr.callee);
            }
            for (size_t i = 0; i < node->as.call_expr.arguments.count; i++) {
                if (i > 0) {
                    fputs(", ", e->out);
                }
                emit_expr(e, node->as.call_expr.arguments.items[i]);
            }
            fputc(')', e->out);
            break;

        case AST_INDEX_EXPR:
            fputs("(((int64_t *)((", e->out);
            emit_expr(e, node->as.index_expr.array);
            fputs(").items))[", e->out);
            emit_expr(e, node->as.index_expr.index);
            fputs("])", e->out);
            break;

        case AST_ARRAY_LITERAL: {
            int id = e->temp++;
            fprintf(e->out, "({ KArr _a%d; _a%d.count = %zu; _a%d.items = calloc(%zu, sizeof(int64_t)); ",
                    id, id, node->as.array_literal.elements.count, id, node->as.array_literal.elements.count);
            for (size_t i = 0; i < node->as.array_literal.elements.count; i++) {
                fprintf(e->out, "((int64_t *)_a%d.items)[%zu] = (int64_t)(", id, i);
                emit_expr(e, node->as.array_literal.elements.items[i]);
                fputs("); ", e->out);
            }
            fprintf(e->out, "_a%d; })", id);
            break;
        }

        default:
            fputs("0", e->out);
            e->had_error = 1;
            break;
    }
}


static void emit_stmt(Emitter *e, const AstNode *node);


static void emit_block_body(Emitter *e, const AstNode *block)
{
    for (size_t i = 0; i < block->as.block.statements.count; i++) {
        emit_stmt(e, block->as.block.statements.items[i]);
    }
}


static void emit_stmt(Emitter *e, const AstNode *node)
{
    if (node == NULL) {
        return;
    }

    switch (node->kind) {
        case AST_VAR_DECL:
            emit_indent(e);
            emit_c_type(e->out, node->as.var_decl.type, node->as.var_decl.is_array);
            fprintf(e->out, " %s = ", node->as.var_decl.name);
            emit_expr(e, node->as.var_decl.initializer);
            fputs(";\n", e->out);
            break;

        case AST_BLOCK:
            emit_indent(e);
            fputs("{\n", e->out);
            e->indent++;
            emit_block_body(e, node);
            e->indent--;
            emit_indent(e);
            fputs("}\n", e->out);
            break;

        case AST_IF_STMT:
            for (size_t i = 0; i < node->as.if_stmt.branches.count; i++) {
                AstNode *branch = node->as.if_stmt.branches.items[i];
                emit_indent(e);
                fputs(i == 0 ? "if (" : "} else if (", e->out);
                emit_expr(e, branch->as.cond_branch.condition);
                fputs(") {\n", e->out);
                e->indent++;
                emit_block_body(e, branch->as.cond_branch.body);
                e->indent--;
            }
            if (node->as.if_stmt.else_block != NULL) {
                emit_indent(e);
                fputs("} else {\n", e->out);
                e->indent++;
                emit_block_body(e, node->as.if_stmt.else_block);
                e->indent--;
            }
            emit_indent(e);
            fputs("}\n", e->out);
            break;

        case AST_HOLD_STMT:
            emit_indent(e);
            fputs("while (", e->out);
            emit_expr(e, node->as.hold_stmt.condition);
            fputs(") {\n", e->out);
            e->indent++;
            emit_block_body(e, node->as.hold_stmt.body);
            e->indent--;
            emit_indent(e);
            fputs("}\n", e->out);
            break;

        case AST_PRESS_STMT:
            emit_indent(e);
            fputs("do {\n", e->out);
            e->indent++;
            emit_block_body(e, node->as.press_stmt.body);
            e->indent--;
            emit_indent(e);
            fputs("} while (", e->out);
            emit_expr(e, node->as.press_stmt.condition);
            fputs(");\n", e->out);
            break;

        case AST_DRIVE_STMT:
            emit_indent(e);
            fputs("{\n", e->out);
            e->indent++;
            emit_stmt(e, node->as.drive_stmt.init);
            emit_indent(e);
            fputs("for (; ", e->out);
            emit_expr(e, node->as.drive_stmt.condition);
            fputs("; ) {\n", e->out);
            e->indent++;
            emit_block_body(e, node->as.drive_stmt.body);
            if (node->as.drive_stmt.step->kind == AST_ASSIGN) {
                emit_stmt(e, node->as.drive_stmt.step);
            } else {
                emit_indent(e);
                emit_expr(e, node->as.drive_stmt.step);
                fputs(";\n", e->out);
            }
            e->indent--;
            emit_indent(e);
            fputs("}\n", e->out);
            e->indent--;
            emit_indent(e);
            fputs("}\n", e->out);
            break;

        case AST_SWEEP_STMT:
            emit_indent(e);
            fprintf(e->out, "for (size_t _i = 0; _i < %s.count; _i++) {\n", node->as.sweep_stmt.collection_name);
            e->indent++;
            emit_indent(e);
            emit_c_type(e->out, node->as.sweep_stmt.element_type, 0);
            fprintf(e->out, " %s = ((int64_t *)%s.items)[_i];\n",
                    node->as.sweep_stmt.element_name,
                    node->as.sweep_stmt.collection_name);
            emit_block_body(e, node->as.sweep_stmt.body);
            e->indent--;
            emit_indent(e);
            fputs("}\n", e->out);
            break;

        case AST_SNAP_STMT:
            emit_indent(e);
            fputs("break;\n", e->out);
            break;

        case AST_PUSH_STMT:
            emit_indent(e);
            fputs("continue;\n", e->out);
            break;

        case AST_YIELD_STMT:
            emit_indent(e);
            if (node->as.yield_stmt.value == NULL) {
                fputs("return;\n", e->out);
            } else {
                fputs("return ", e->out);
                emit_expr(e, node->as.yield_stmt.value);
                fputs(";\n", e->out);
            }
            break;

        case AST_SHOUT_STMT:
            emit_indent(e);
            fputs("k_shout(", e->out);
            emit_expr(e, node->as.shout_stmt.value);
            fputs(");\n", e->out);
            break;

        case AST_ASSIGN:
            emit_indent(e);
            emit_expr(e, node->as.assign.target);
            fputs(" = ", e->out);
            emit_expr(e, node->as.assign.value);
            fputs(";\n", e->out);
            break;

        case AST_EXPR_STMT:
            emit_indent(e);
            emit_expr(e, node->as.expr_stmt.expression);
            fputs(";\n", e->out);
            break;

        default:
            emit_indent(e);
            fputs("/* istruzione non emessa */;\n", e->out);
            e->had_error = 1;
            break;
    }
}


static void emit_function(Emitter *e, const AstNode *func)
{
    emit_c_type(e->out, func->as.func_decl.return_type, 0);
    fprintf(e->out, " kfn_%s(", func->as.func_decl.name);
    if (func->as.func_decl.params.count == 0) {
        fputs("void", e->out);
    } else {
        for (size_t i = 0; i < func->as.func_decl.params.count; i++) {
            if (i > 0) {
                fputs(", ", e->out);
            }
            AstNode *param = func->as.func_decl.params.items[i];
            emit_c_type(e->out, param->as.param.type, 0);
            fprintf(e->out, " %s", param->as.param.name);
        }
    }
    fputs(")\n{\n", e->out);
    e->indent = 1;
    emit_block_body(e, func->as.func_decl.body);
    e->indent = 0;
    fputs("}\n\n", e->out);
}


int codegen_emit_c(const AstNode *program, FILE *out)
{
    Emitter e;
    e.out = out;
    e.indent = 0;
    e.temp = 0;
    e.had_error = 0;

    fputs(
        "/* Generated by kratosc --emit-c */\n"
        "#include <stdio.h>\n"
        "#include <stdint.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n\n"
        "typedef struct { void *items; size_t count; } KArr;\n\n"
        "#define k_shout(v) _Generic((v), \\\n"
        "    int64_t: k_shout_int, \\\n"
        "    int: k_shout_int32, \\\n"
        "    double: k_shout_float, \\\n"
        "    char: k_shout_char, \\\n"
        "    char *: k_shout_string, \\\n"
        "    const char *: k_shout_string, \\\n"
        "    KArr: k_shout_arr, \\\n"
        "    default: k_shout_int \\\n"
        ")(v)\n\n"
        "static void k_shout_int(int64_t v) { printf(\"%lld\\n\", (long long)v); }\n"
        "static void k_shout_int32(int v) { printf(\"%d\\n\", v); }\n"
        "static void k_shout_float(double v) { printf(\"%g\\n\", v); }\n"
        "static void k_shout_char(char v) { printf(\"%c\\n\", v); }\n"
        "static void k_shout_string(const char *v) { printf(\"%s\\n\", v ? v : \"\"); }\n"
        "static void k_shout_arr(KArr v) {\n"
        "    printf(\"[\");\n"
        "    for (size_t i = 0; i < v.count; i++) {\n"
        "        if (i) printf(\", \");\n"
        "        printf(\"%lld\", (long long)((int64_t *)v.items)[i]);\n"
        "    }\n"
        "    printf(\"]\\n\");\n"
        "}\n"
        "static int64_t k_len_string(const char *v) { return (int64_t)strlen(v ? v : \"\"); }\n"
        "static int64_t k_len_array(KArr v) { return (int64_t)v.count; }\n"
        "#define k_len(v) _Generic((v), \\\n"
        "    KArr: k_len_array, \\\n"
        "    char *: k_len_string, \\\n"
        "    const char *: k_len_string \\\n"
        ")(v)\n\n",
        out
    );

    int has_main = 0;
    for (size_t i = 0; i < program->as.program.declarations.count; i++) {
        const AstNode *node = program->as.program.declarations.items[i];
        if (node->kind == AST_FUNC_DECL) {
            emit_c_type(out, node->as.func_decl.return_type, 0);
            fprintf(out, " kfn_%s(", node->as.func_decl.name);
            if (node->as.func_decl.params.count == 0) {
                fputs("void", out);
            } else {
                for (size_t p = 0; p < node->as.func_decl.params.count; p++) {
                    if (p > 0) {
                        fputs(", ", out);
                    }
                    emit_c_type(out, node->as.func_decl.params.items[p]->as.param.type, 0);
                }
            }
            fputs(");\n", out);
            if (strcmp(node->as.func_decl.name, "main") == 0) {
                has_main = 1;
            }
        }
    }
    fputc('\n', out);

    for (size_t i = 0; i < program->as.program.declarations.count; i++) {
        const AstNode *node = program->as.program.declarations.items[i];
        if (node->kind == AST_VAR_DECL) {
            emit_c_type(out, node->as.var_decl.type, node->as.var_decl.is_array);
            fprintf(out, " %s;\n", node->as.var_decl.name);
        }
    }
    fputc('\n', out);

    fputs("static void kratos_init(void)\n{\n", out);
    e.indent = 1;
    for (size_t i = 0; i < program->as.program.declarations.count; i++) {
        const AstNode *node = program->as.program.declarations.items[i];
        if (node->kind == AST_VAR_DECL) {
            emit_indent(&e);
            fprintf(out, "%s = ", node->as.var_decl.name);
            emit_expr(&e, node->as.var_decl.initializer);
            fputs(";\n", out);
        }
    }
    e.indent = 0;
    fputs("}\n\n", out);

    for (size_t i = 0; i < program->as.program.declarations.count; i++) {
        const AstNode *node = program->as.program.declarations.items[i];
        if (node->kind == AST_FUNC_DECL) {
            emit_function(&e, node);
        }
    }

    fputs("int main(void)\n{\n    kratos_init();\n", out);
    if (has_main) {
        fputs("    kfn_main();\n", out);
    }
    fputs("    return 0;\n}\n", out);

    return e.had_error ? 1 : 0;
}
