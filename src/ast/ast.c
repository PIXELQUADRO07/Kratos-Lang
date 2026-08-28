#include "ast/ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- AstNodeList --- */

void ast_node_list_init(AstNodeList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_node_list_push(AstNodeList *list, AstNode *node)
{
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        AstNode **new_items = realloc(list->items, new_capacity * sizeof(AstNode *));
        if (new_items == NULL) {
            fprintf(stderr, "kratos: memoria esaurita durante la costruzione dell'AST\n");
            exit(EXIT_FAILURE);
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count] = node;
    list->count += 1;
}

void ast_node_list_free(AstNodeList *list)
{
    for (size_t i = 0; i < list->count; i++) {
        ast_free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* --- Utility interne --- */

static char *duplicate_string(const char *source)
{
    if (source == NULL) {
        return NULL;
    }

    size_t length = strlen(source);
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        fprintf(stderr, "kratos: memoria esaurita durante la costruzione dell'AST\n");
        exit(EXIT_FAILURE);
    }

    memcpy(copy, source, length + 1);
    return copy;
}

static AstNode *new_node(AstNodeKind kind, size_t line)
{
    AstNode *node = calloc(1, sizeof(AstNode));
    if (node == NULL) {
        fprintf(stderr, "kratos: memoria esaurita durante la costruzione dell'AST\n");
        exit(EXIT_FAILURE);
    }

    node->kind = kind;
    node->line = line;
    node->column = 1;
    node->length = 1;
    return node;
}

void ast_set_span(AstNode *node, size_t line, size_t column, size_t length)
{
    if (node == NULL) {
        return;
    }
    node->line = line;
    node->column = column == 0 ? 1 : column;
    node->length = length == 0 ? 1 : length;
}

/* --- Costruttori --- */

AstNode *ast_new_program(size_t line)
{
    AstNode *node = new_node(AST_PROGRAM, line);
    ast_node_list_init(&node->as.program.declarations);
    return node;
}

AstNode *ast_new_var_decl(size_t line, int is_const, int is_array, KratosType type, const char *name, AstNode *initializer)
{
    AstNode *node = new_node(AST_VAR_DECL, line);
    node->as.var_decl.is_const = is_const;
    node->as.var_decl.is_array = is_array;
    node->as.var_decl.type = type;
    node->as.var_decl.name = duplicate_string(name);
    node->as.var_decl.initializer = initializer;
    return node;
}

AstNode *ast_new_func_decl(size_t line, KratosType return_type, const char *name, AstNode *body)
{
    AstNode *node = new_node(AST_FUNC_DECL, line);
    node->as.func_decl.return_type = return_type;
    node->as.func_decl.name = duplicate_string(name);
    ast_node_list_init(&node->as.func_decl.params);
    node->as.func_decl.body = body;
    return node;
}

AstNode *ast_new_param(size_t line, KratosType type, const char *name)
{
    AstNode *node = new_node(AST_PARAM, line);
    node->as.param.type = type;
    node->as.param.name = duplicate_string(name);
    return node;
}

AstNode *ast_new_block(size_t line)
{
    AstNode *node = new_node(AST_BLOCK, line);
    ast_node_list_init(&node->as.block.statements);
    return node;
}

AstNode *ast_new_if_stmt(size_t line)
{
    AstNode *node = new_node(AST_IF_STMT, line);
    ast_node_list_init(&node->as.if_stmt.branches);
    node->as.if_stmt.else_block = NULL;
    return node;
}

AstNode *ast_new_cond_branch(size_t line, AstNode *condition, AstNode *body)
{
    AstNode *node = new_node(AST_COND_BRANCH, line);
    node->as.cond_branch.condition = condition;
    node->as.cond_branch.body = body;
    return node;
}

AstNode *ast_new_hold_stmt(size_t line, AstNode *condition, AstNode *body)
{
    AstNode *node = new_node(AST_HOLD_STMT, line);
    node->as.hold_stmt.condition = condition;
    node->as.hold_stmt.body = body;
    return node;
}

AstNode *ast_new_press_stmt(size_t line, AstNode *body, AstNode *condition)
{
    AstNode *node = new_node(AST_PRESS_STMT, line);
    node->as.press_stmt.body = body;
    node->as.press_stmt.condition = condition;
    return node;
}

AstNode *ast_new_drive_stmt(size_t line, AstNode *init, AstNode *condition, AstNode *step, AstNode *body)
{
    AstNode *node = new_node(AST_DRIVE_STMT, line);
    node->as.drive_stmt.init = init;
    node->as.drive_stmt.condition = condition;
    node->as.drive_stmt.step = step;
    node->as.drive_stmt.body = body;
    return node;
}

AstNode *ast_new_sweep_stmt(size_t line, KratosType element_type, const char *element_name, const char *collection_name, AstNode *body)
{
    AstNode *node = new_node(AST_SWEEP_STMT, line);
    node->as.sweep_stmt.element_type = element_type;
    node->as.sweep_stmt.element_name = duplicate_string(element_name);
    node->as.sweep_stmt.collection_name = duplicate_string(collection_name);
    node->as.sweep_stmt.body = body;
    return node;
}

AstNode *ast_new_snap_stmt(size_t line)
{
    return new_node(AST_SNAP_STMT, line);
}

AstNode *ast_new_push_stmt(size_t line)
{
    return new_node(AST_PUSH_STMT, line);
}

AstNode *ast_new_yield_stmt(size_t line, AstNode *value)
{
    AstNode *node = new_node(AST_YIELD_STMT, line);
    node->as.yield_stmt.value = value;
    return node;
}

AstNode *ast_new_shout_stmt(size_t line, AstNode *value)
{
    AstNode *node = new_node(AST_SHOUT_STMT, line);
    node->as.shout_stmt.value = value;
    return node;
}

AstNode *ast_new_wield_stmt(size_t line, const char *path)
{
    AstNode *node = new_node(AST_WIELD_STMT, line);
    node->as.wield_stmt.path = duplicate_string(path);
    return node;
}

AstNode *ast_new_assign(size_t line, const char *target, AstNode *value)
{
    AstNode *node = new_node(AST_ASSIGN, line);
    node->as.assign.target = duplicate_string(target);
    node->as.assign.value = value;
    return node;
}

AstNode *ast_new_expr_stmt(size_t line, AstNode *expression)
{
    AstNode *node = new_node(AST_EXPR_STMT, line);
    node->as.expr_stmt.expression = expression;
    return node;
}

AstNode *ast_new_binary_expr(size_t line, TokenType operator, AstNode *left, AstNode *right)
{
    AstNode *node = new_node(AST_BINARY_EXPR, line);
    node->as.binary_expr.operator = operator;
    node->as.binary_expr.left = left;
    node->as.binary_expr.right = right;
    return node;
}

AstNode *ast_new_identifier_expr(size_t line, const char *name)
{
    AstNode *node = new_node(AST_IDENTIFIER_EXPR, line);
    node->as.identifier_expr.name = duplicate_string(name);
    return node;
}

AstNode *ast_new_literal_int(size_t line, int64_t value)
{
    AstNode *node = new_node(AST_LITERAL_EXPR, line);
    node->as.literal_expr.kind = LITERAL_INT;
    node->as.literal_expr.value.int_value = value;
    return node;
}

AstNode *ast_new_literal_float(size_t line, double value)
{
    AstNode *node = new_node(AST_LITERAL_EXPR, line);
    node->as.literal_expr.kind = LITERAL_FLOAT;
    node->as.literal_expr.value.float_value = value;
    return node;
}

AstNode *ast_new_literal_bool(size_t line, int value)
{
    AstNode *node = new_node(AST_LITERAL_EXPR, line);
    node->as.literal_expr.kind = LITERAL_BOOL;
    node->as.literal_expr.value.bool_value = value ? 1 : 0;
    return node;
}

AstNode *ast_new_literal_char(size_t line, char value)
{
    AstNode *node = new_node(AST_LITERAL_EXPR, line);
    node->as.literal_expr.kind = LITERAL_CHAR;
    node->as.literal_expr.value.char_value = value;
    return node;
}

AstNode *ast_new_literal_string(size_t line, const char *value)
{
    AstNode *node = new_node(AST_LITERAL_EXPR, line);
    node->as.literal_expr.kind = LITERAL_STRING;
    node->as.literal_expr.value.string_value = duplicate_string(value);
    return node;
}

AstNode *ast_new_call_expr(size_t line, const char *callee)
{
    AstNode *node = new_node(AST_CALL_EXPR, line);
    node->as.call_expr.callee = duplicate_string(callee);
    ast_node_list_init(&node->as.call_expr.arguments);
    return node;
}

AstNode *ast_new_array_literal(size_t line)
{
    AstNode *node = new_node(AST_ARRAY_LITERAL, line);
    ast_node_list_init(&node->as.array_literal.elements);
    return node;
}

AstNode *ast_new_index_expr(size_t line, AstNode *array, AstNode *index)
{
    AstNode *node = new_node(AST_INDEX_EXPR, line);
    node->as.index_expr.array = array;
    node->as.index_expr.index = index;
    return node;
}

AstNode *ast_new_unary_expr(size_t line, TokenType operator, AstNode *operand)
{
    AstNode *node = new_node(AST_UNARY_EXPR, line);
    node->as.unary_expr.operator = operator;
    node->as.unary_expr.operand = operand;
    return node;
}

/* --- Distruzione --- */

void ast_free(AstNode *node)
{
    if (node == NULL) {
        return;
    }

    switch (node->kind) {
        case AST_PROGRAM:
            ast_node_list_free(&node->as.program.declarations);
            break;

        case AST_VAR_DECL:
            free(node->as.var_decl.name);
            ast_free(node->as.var_decl.initializer);
            break;

        case AST_FUNC_DECL:
            free(node->as.func_decl.name);
            ast_node_list_free(&node->as.func_decl.params);
            ast_free(node->as.func_decl.body);
            break;

        case AST_PARAM:
            free(node->as.param.name);
            break;

        case AST_BLOCK:
            ast_node_list_free(&node->as.block.statements);
            break;

        case AST_IF_STMT:
            ast_node_list_free(&node->as.if_stmt.branches);
            ast_free(node->as.if_stmt.else_block);
            break;

        case AST_COND_BRANCH:
            ast_free(node->as.cond_branch.condition);
            ast_free(node->as.cond_branch.body);
            break;

        case AST_HOLD_STMT:
            ast_free(node->as.hold_stmt.condition);
            ast_free(node->as.hold_stmt.body);
            break;

        case AST_PRESS_STMT:
            ast_free(node->as.press_stmt.body);
            ast_free(node->as.press_stmt.condition);
            break;

        case AST_DRIVE_STMT:
            ast_free(node->as.drive_stmt.init);
            ast_free(node->as.drive_stmt.condition);
            ast_free(node->as.drive_stmt.step);
            ast_free(node->as.drive_stmt.body);
            break;

        case AST_SWEEP_STMT:
            free(node->as.sweep_stmt.element_name);
            free(node->as.sweep_stmt.collection_name);
            ast_free(node->as.sweep_stmt.body);
            break;

        case AST_SNAP_STMT:
        case AST_PUSH_STMT:
            break;

        case AST_YIELD_STMT:
            ast_free(node->as.yield_stmt.value);
            break;

        case AST_SHOUT_STMT:
            ast_free(node->as.shout_stmt.value);
            break;

        case AST_WIELD_STMT:
            free(node->as.wield_stmt.path);
            break;

        case AST_ASSIGN:
            free(node->as.assign.target);
            ast_free(node->as.assign.value);
            break;

        case AST_EXPR_STMT:
            ast_free(node->as.expr_stmt.expression);
            break;

        case AST_BINARY_EXPR:
            ast_free(node->as.binary_expr.left);
            ast_free(node->as.binary_expr.right);
            break;

        case AST_IDENTIFIER_EXPR:
            free(node->as.identifier_expr.name);
            break;

        case AST_LITERAL_EXPR:
            if (node->as.literal_expr.kind == LITERAL_STRING) {
                free(node->as.literal_expr.value.string_value);
            }
            break;

        case AST_CALL_EXPR:
            free(node->as.call_expr.callee);
            ast_node_list_free(&node->as.call_expr.arguments);
            break;

        case AST_ARRAY_LITERAL:
            ast_node_list_free(&node->as.array_literal.elements);
            break;

        case AST_INDEX_EXPR:
            ast_free(node->as.index_expr.array);
            ast_free(node->as.index_expr.index);
            break;

        case AST_UNARY_EXPR:
            ast_free(node->as.unary_expr.operand);
            break;
    }

    free(node);
}

/* --- Debug --- */

const char *kratos_type_name(KratosType type)
{
    switch (type) {
        case KRATOS_TYPE_INT:    return "k_int";
        case KRATOS_TYPE_FLOAT:  return "k_float";
        case KRATOS_TYPE_BOOL:   return "k_bool";
        case KRATOS_TYPE_CHAR:   return "k_char";
        case KRATOS_TYPE_STRING: return "k_string";
        case KRATOS_TYPE_VOID:   return "k_void";
    }
    return "?";
}

static void print_indent(int indent)
{
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print(const AstNode *node, int indent)
{
    if (node == NULL) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    print_indent(indent);

    switch (node->kind) {
        case AST_PROGRAM:
            printf("Program\n");
            for (size_t i = 0; i < node->as.program.declarations.count; i++) {
                ast_print(node->as.program.declarations.items[i], indent + 1);
            }
            break;

        case AST_VAR_DECL:
            printf(
                "VarDecl %s%s : %s%s\n",
                node->as.var_decl.is_const ? "const " : "",
                node->as.var_decl.name,
                kratos_type_name(node->as.var_decl.type),
                node->as.var_decl.is_array ? "[]" : ""
            );
            ast_print(node->as.var_decl.initializer, indent + 1);
            break;

        case AST_FUNC_DECL:
            printf(
                "FuncDecl %s -> %s\n",
                node->as.func_decl.name,
                kratos_type_name(node->as.func_decl.return_type)
            );
            for (size_t i = 0; i < node->as.func_decl.params.count; i++) {
                ast_print(node->as.func_decl.params.items[i], indent + 1);
            }
            ast_print(node->as.func_decl.body, indent + 1);
            break;

        case AST_PARAM:
            printf("Param %s : %s\n", node->as.param.name, kratos_type_name(node->as.param.type));
            break;

        case AST_BLOCK:
            printf("Block\n");
            for (size_t i = 0; i < node->as.block.statements.count; i++) {
                ast_print(node->as.block.statements.items[i], indent + 1);
            }
            break;

        case AST_IF_STMT:
            printf("If\n");
            for (size_t i = 0; i < node->as.if_stmt.branches.count; i++) {
                ast_print(node->as.if_stmt.branches.items[i], indent + 1);
            }
            if (node->as.if_stmt.else_block != NULL) {
                print_indent(indent + 1);
                printf("Else\n");
                ast_print(node->as.if_stmt.else_block, indent + 2);
            }
            break;

        case AST_COND_BRANCH:
            printf("Branch\n");
            ast_print(node->as.cond_branch.condition, indent + 1);
            ast_print(node->as.cond_branch.body, indent + 1);
            break;

        case AST_HOLD_STMT:
            printf("Hold\n");
            ast_print(node->as.hold_stmt.condition, indent + 1);
            ast_print(node->as.hold_stmt.body, indent + 1);
            break;

        case AST_PRESS_STMT:
            printf("Press\n");
            ast_print(node->as.press_stmt.body, indent + 1);
            ast_print(node->as.press_stmt.condition, indent + 1);
            break;

        case AST_DRIVE_STMT:
            printf("Drive\n");
            ast_print(node->as.drive_stmt.init, indent + 1);
            ast_print(node->as.drive_stmt.condition, indent + 1);
            ast_print(node->as.drive_stmt.step, indent + 1);
            ast_print(node->as.drive_stmt.body, indent + 1);
            break;

        case AST_SWEEP_STMT:
            printf(
                "Sweep %s : %s in %s\n",
                node->as.sweep_stmt.element_name,
                kratos_type_name(node->as.sweep_stmt.element_type),
                node->as.sweep_stmt.collection_name
            );
            ast_print(node->as.sweep_stmt.body, indent + 1);
            break;

        case AST_SNAP_STMT:
            printf("Snap\n");
            break;

        case AST_PUSH_STMT:
            printf("Push\n");
            break;

        case AST_YIELD_STMT:
            printf("Yield\n");
            if (node->as.yield_stmt.value != NULL) {
                ast_print(node->as.yield_stmt.value, indent + 1);
            }
            break;

        case AST_SHOUT_STMT:
            printf("Shout\n");
            ast_print(node->as.shout_stmt.value, indent + 1);
            break;

        case AST_WIELD_STMT:
            printf("Wield \"%s\"\n", node->as.wield_stmt.path);
            break;

        case AST_ASSIGN:
            printf("Assign %s =\n", node->as.assign.target);
            ast_print(node->as.assign.value, indent + 1);
            break;

        case AST_EXPR_STMT:
            printf("ExprStmt\n");
            ast_print(node->as.expr_stmt.expression, indent + 1);
            break;

        case AST_BINARY_EXPR:
            printf("Binary %s\n", token_type_name(node->as.binary_expr.operator));
            ast_print(node->as.binary_expr.left, indent + 1);
            ast_print(node->as.binary_expr.right, indent + 1);
            break;

        case AST_IDENTIFIER_EXPR:
            printf("Identifier %s\n", node->as.identifier_expr.name);
            break;

        case AST_LITERAL_EXPR:
            switch (node->as.literal_expr.kind) {
                case LITERAL_INT:
                    printf("Literal %lld\n", (long long)node->as.literal_expr.value.int_value);
                    break;
                case LITERAL_FLOAT:
                    printf("Literal %g\n", node->as.literal_expr.value.float_value);
                    break;
                case LITERAL_BOOL:
                    printf("Literal %s\n", node->as.literal_expr.value.bool_value ? "true" : "false");
                    break;
                case LITERAL_CHAR:
                    printf("Literal '%c'\n", node->as.literal_expr.value.char_value);
                    break;
                case LITERAL_STRING:
                    printf("Literal \"%s\"\n", node->as.literal_expr.value.string_value);
                    break;
            }
            break;

        case AST_CALL_EXPR:
            printf("Call %s\n", node->as.call_expr.callee);
            for (size_t i = 0; i < node->as.call_expr.arguments.count; i++) {
                ast_print(node->as.call_expr.arguments.items[i], indent + 1);
            }
            break;

        case AST_ARRAY_LITERAL:
            printf("ArrayLiteral\n");
            for (size_t i = 0; i < node->as.array_literal.elements.count; i++) {
                ast_print(node->as.array_literal.elements.items[i], indent + 1);
            }
            break;

        case AST_INDEX_EXPR:
            printf("Index\n");
            ast_print(node->as.index_expr.array, indent + 1);
            ast_print(node->as.index_expr.index, indent + 1);
            break;

        case AST_UNARY_EXPR:
            printf("Unary %s\n", token_type_name(node->as.unary_expr.operator));
            ast_print(node->as.unary_expr.operand, indent + 1);
            break;
    }
}
