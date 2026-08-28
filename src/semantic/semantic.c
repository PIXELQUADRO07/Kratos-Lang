#include "semantic/semantic.h"

#include "diag/diag.h"
#include "parser/parser.h"
#include "utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    KratosType type;
    int is_array;
} TypeInfo;

typedef struct Symbol {
    char *name;
    TypeInfo type;
    int is_const;
    int is_function;
    AstNode *func;
} Symbol;

typedef struct Scope {
    Symbol *items;
    size_t count;
    size_t capacity;
    struct Scope *parent;
} Scope;

typedef struct {
    int had_error;
    Scope *scope;
    int loop_depth;
    int in_function;
    KratosType current_return;
    char **wield_stack;
    size_t wield_stack_count;
    size_t wield_stack_capacity;
    const DiagContext *diag_context;
} Analyzer;


static TypeInfo type_info(KratosType type, int is_array)
{
    TypeInfo info;
    info.type = type;
    info.is_array = is_array;
    return info;
}


static void semantic_error(Analyzer *analyzer, size_t line, const char *message)
{
    int code = DIAG_K304;
    if (strstr(message, "gia' dichiarato") != NULL) {
        code = DIAG_K301;
    } else if (strstr(message, "dichiarato") != NULL || strstr(message, "dichiarata") != NULL) {
        code = DIAG_K302;
    } else if (strstr(message, "craft") != NULL && strstr(message, "valore") != NULL) {
        code = DIAG_K303;
    } else if (strstr(message, "condizion") != NULL) {
        code = DIAG_K305;
    } else if (strstr(message, "snap/push") != NULL) {
        code = DIAG_K306;
    } else if (strstr(message, "yield") != NULL) {
        code = DIAG_K307;
    } else if (strstr(message, "wield") != NULL || strstr(message, "file di") != NULL) {
        code = DIAG_K308;
    } else if (strstr(message, "k_const") != NULL) {
        code = DIAG_K309;
    } else if (strstr(message, "ogni cammino") != NULL) {
        code = DIAG_K310;
    } else if (strstr(message, "k_void") != NULL) {
        code = DIAG_K311;
    } else if (strstr(message, "livello") != NULL) {
        code = DIAG_K312;
    } else if (strstr(message, "array") != NULL) {
        code = DIAG_K313;
    } else if (strstr(message, "craft inesistente") != NULL || strstr(message, "argomenti") != NULL || strstr(message, "chiamata") != NULL) {
        code = DIAG_K314;
    }
    const char *display_message = message;
    switch (code) {
        case DIAG_K301: display_message = "name already declared in this scope"; break;
        case DIAG_K302: display_message = "undeclared identifier"; break;
        case DIAG_K303: display_message = "a craft is not a value"; break;
        case DIAG_K304: display_message = "type mismatch"; break;
        case DIAG_K305: display_message = "condition must be k_bool"; break;
        case DIAG_K306: display_message = "snap and push are only valid inside a loop"; break;
        case DIAG_K307: display_message = "invalid yield"; break;
        case DIAG_K308: display_message = "wield error"; break;
        case DIAG_K309: display_message = "cannot assign to k_const"; break;
        case DIAG_K310: display_message = "craft must yield on every path"; break;
        case DIAG_K311: display_message = "k_void cannot declare a variable or parameter"; break;
        case DIAG_K312: display_message = "invalid construct at this level"; break;
        case DIAG_K313: display_message = "nested arrays are not supported"; break;
        case DIAG_K314: display_message = "invalid function call"; break;
        default: break;
    }
    diag_emitf(analyzer->diag_context, DIAG_ERROR, code, line, 1, 1, NULL, "%s", display_message);
    analyzer->had_error = 1;
}


static void scope_init(Scope *scope, Scope *parent)
{
    scope->items = NULL;
    scope->count = 0;
    scope->capacity = 0;
    scope->parent = parent;
}


static void scope_free(Scope *scope)
{
    for (size_t i = 0; i < scope->count; i++) {
        free(scope->items[i].name);
    }
    free(scope->items);
    scope->items = NULL;
    scope->count = 0;
    scope->capacity = 0;
}


static Symbol *scope_find_local(Scope *scope, const char *name)
{
    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->items[i].name, name) == 0) {
            return &scope->items[i];
        }
    }
    return NULL;
}


static Symbol *scope_find(Scope *scope, const char *name)
{
    for (Scope *current = scope; current != NULL; current = current->parent) {
        Symbol *found = scope_find_local(current, name);
        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}


static void scope_add(Analyzer *analyzer, size_t line, const char *name, TypeInfo type, int is_const, int is_function, AstNode *func)
{
    if (scope_find_local(analyzer->scope, name) != NULL) {
        semantic_error(analyzer, line, "nome gia' dichiarato in questo ambito");
        return;
    }

    if (analyzer->scope->count == analyzer->scope->capacity) {
        size_t new_capacity = analyzer->scope->capacity == 0 ? 8 : analyzer->scope->capacity * 2;
        Symbol *new_items = realloc(analyzer->scope->items, new_capacity * sizeof(Symbol));
        if (new_items == NULL) {
            fprintf(stderr, "kratos: memoria esaurita\n");
            exit(EXIT_FAILURE);
        }
        analyzer->scope->items = new_items;
        analyzer->scope->capacity = new_capacity;
    }

    Symbol *symbol = &analyzer->scope->items[analyzer->scope->count++];
    symbol->name = kratos_copy_string(name);
    symbol->type = type;
    symbol->is_const = is_const;
    symbol->is_function = is_function;
    symbol->func = func;
}


static int types_equal(TypeInfo a, TypeInfo b)
{
    return a.type == b.type && a.is_array == b.is_array;
}


static int assignable(TypeInfo from, TypeInfo to)
{
    if (types_equal(from, to)) {
        return 1;
    }
    if (!from.is_array && !to.is_array && from.type == KRATOS_TYPE_INT && to.type == KRATOS_TYPE_FLOAT) {
        return 1;
    }
    return 0;
}


static int always_returns(const AstNode *node)
{
    if (node == NULL) {
        return 0;
    }

    switch (node->kind) {
        case AST_YIELD_STMT:
            return 1;

        case AST_BLOCK: {
            for (size_t i = 0; i < node->as.block.statements.count; i++) {
                if (always_returns(node->as.block.statements.items[i])) {
                    return 1;
                }
            }
            return 0;
        }

        case AST_IF_STMT: {
            if (node->as.if_stmt.else_block == NULL) {
                return 0;
            }
            for (size_t i = 0; i < node->as.if_stmt.branches.count; i++) {
                if (!always_returns(node->as.if_stmt.branches.items[i]->as.cond_branch.body)) {
                    return 0;
                }
            }
            return always_returns(node->as.if_stmt.else_block);
        }

        default:
            return 0;
    }
}


static TypeInfo check_expr(Analyzer *analyzer, AstNode *node);
static void check_stmt(Analyzer *analyzer, AstNode *node);


static TypeInfo check_expr(Analyzer *analyzer, AstNode *node)
{
    TypeInfo invalid = type_info(KRATOS_TYPE_VOID, 0);
    if (node == NULL) {
        return invalid;
    }

    switch (node->kind) {
        case AST_LITERAL_EXPR:
            switch (node->as.literal_expr.kind) {
                case LITERAL_INT:    return type_info(KRATOS_TYPE_INT, 0);
                case LITERAL_FLOAT:  return type_info(KRATOS_TYPE_FLOAT, 0);
                case LITERAL_BOOL:   return type_info(KRATOS_TYPE_BOOL, 0);
                case LITERAL_CHAR:   return type_info(KRATOS_TYPE_CHAR, 0);
                case LITERAL_STRING: return type_info(KRATOS_TYPE_STRING, 0);
            }
            break;

        case AST_IDENTIFIER_EXPR: {
            Symbol *symbol = scope_find(analyzer->scope, node->as.identifier_expr.name);
            if (symbol == NULL) {
                semantic_error(analyzer, node->line, "identificatore non dichiarato");
                return invalid;
            }
            if (symbol->is_function) {
                semantic_error(analyzer, node->line, "una craft non e' un valore");
                return invalid;
            }
            return symbol->type;
        }

        case AST_UNARY_EXPR: {
            TypeInfo operand = check_expr(analyzer, node->as.unary_expr.operand);
            if (node->as.unary_expr.operator == TOKEN_NOT) {
                if (operand.is_array || operand.type != KRATOS_TYPE_BOOL) {
                    semantic_error(analyzer, node->line, "'not' richiede un k_bool");
                }
                return type_info(KRATOS_TYPE_BOOL, 0);
            }
            if (operand.is_array || (operand.type != KRATOS_TYPE_INT && operand.type != KRATOS_TYPE_FLOAT)) {
                semantic_error(analyzer, node->line, "negazione aritmetica richiede k_int o k_float");
            }
            return operand;
        }

        case AST_BINARY_EXPR: {
            TokenType op = node->as.binary_expr.operator;
            TypeInfo left = check_expr(analyzer, node->as.binary_expr.left);

            if (op == TOKEN_AND || op == TOKEN_OR) {
                if (left.type != KRATOS_TYPE_BOOL || left.is_array) {
                    semantic_error(analyzer, node->line, "&& e || richiedono k_bool");
                }
                TypeInfo right = check_expr(analyzer, node->as.binary_expr.right);
                if (right.type != KRATOS_TYPE_BOOL || right.is_array) {
                    semantic_error(analyzer, node->line, "&& e || richiedono k_bool");
                }
                return type_info(KRATOS_TYPE_BOOL, 0);
            }

            TypeInfo right = check_expr(analyzer, node->as.binary_expr.right);

            if (op == TOKEN_PLUS && !left.is_array && !right.is_array &&
                left.type == KRATOS_TYPE_STRING && right.type == KRATOS_TYPE_STRING) {
                return type_info(KRATOS_TYPE_STRING, 0);
            }

            if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL) {
                if (left.is_array || right.is_array || !types_equal(left, right)) {
                    semantic_error(analyzer, node->line, "confronto tra tipi incompatibili");
                }
                return type_info(KRATOS_TYPE_BOOL, 0);
            }

            if (op == TOKEN_LESS || op == TOKEN_GREATER || op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {
                if (left.is_array || right.is_array ||
                    (left.type != KRATOS_TYPE_INT && left.type != KRATOS_TYPE_FLOAT) ||
                    (right.type != KRATOS_TYPE_INT && right.type != KRATOS_TYPE_FLOAT)) {
                    semantic_error(analyzer, node->line, "confronto relazionale richiede k_int o k_float");
                }
                return type_info(KRATOS_TYPE_BOOL, 0);
            }

            if (op == TOKEN_PERCENT) {
                if (left.is_array || right.is_array || left.type != KRATOS_TYPE_INT || right.type != KRATOS_TYPE_INT) {
                    semantic_error(analyzer, node->line, "% richiede due k_int");
                }
                return type_info(KRATOS_TYPE_INT, 0);
            }

            if (left.is_array || right.is_array ||
                (left.type != KRATOS_TYPE_INT && left.type != KRATOS_TYPE_FLOAT) ||
                (right.type != KRATOS_TYPE_INT && right.type != KRATOS_TYPE_FLOAT)) {
                semantic_error(analyzer, node->line, "operatore aritmetico richiede k_int o k_float");
                return type_info(KRATOS_TYPE_INT, 0);
            }

            if (left.type == KRATOS_TYPE_FLOAT || right.type == KRATOS_TYPE_FLOAT) {
                return type_info(KRATOS_TYPE_FLOAT, 0);
            }
            return type_info(KRATOS_TYPE_INT, 0);
        }

        case AST_CALL_EXPR: {
            if (strcmp(node->as.call_expr.callee, "len") == 0) {
                if (node->as.call_expr.arguments.count != 1) {
                    semantic_error(analyzer, node->line, "len richiede un argomento");
                    return invalid;
                }
                TypeInfo argument = check_expr(analyzer, node->as.call_expr.arguments.items[0]);
                if (!argument.is_array && argument.type != KRATOS_TYPE_STRING) {
                    semantic_error(analyzer, node->line, "len richiede una stringa o un array");
                }
                return type_info(KRATOS_TYPE_INT, 0);
            }
            if (strcmp(node->as.call_expr.callee, "slice") == 0) {
                if (node->as.call_expr.arguments.count != 3) {
                    semantic_error(analyzer, node->line, "slice richiede tre argomenti");
                    return invalid;
                }
                TypeInfo source = check_expr(analyzer, node->as.call_expr.arguments.items[0]);
                TypeInfo start = check_expr(analyzer, node->as.call_expr.arguments.items[1]);
                TypeInfo end = check_expr(analyzer, node->as.call_expr.arguments.items[2]);
                if ((source.is_array == 0 && source.type != KRATOS_TYPE_STRING) || source.is_array > 1) {
                    semantic_error(analyzer, node->line, "slice richiede una stringa o un array semplice");
                }
                if (start.is_array || start.type != KRATOS_TYPE_INT ||
                    end.is_array || end.type != KRATOS_TYPE_INT) {
                    semantic_error(analyzer, node->line, "gli indici di slice devono essere k_int");
                }
                return source.is_array == 1 ? type_info(source.type, 1) : type_info(KRATOS_TYPE_STRING, 0);
            }
            if (strcmp(node->as.call_expr.callee, "to_string") == 0 ||
                strcmp(node->as.call_expr.callee, "to_int") == 0 ||
                strcmp(node->as.call_expr.callee, "to_float") == 0) {
                if (node->as.call_expr.arguments.count != 1) {
                    semantic_error(analyzer, node->line, "la conversione richiede un argomento");
                    return invalid;
                }
                TypeInfo argument = check_expr(analyzer, node->as.call_expr.arguments.items[0]);
                if (argument.is_array || argument.type == KRATOS_TYPE_VOID) {
                    semantic_error(analyzer, node->line, "la conversione richiede un valore scalare");
                }
                if (strcmp(node->as.call_expr.callee, "to_string") == 0) {
                    return type_info(KRATOS_TYPE_STRING, 0);
                }
                if (argument.type != KRATOS_TYPE_INT && argument.type != KRATOS_TYPE_FLOAT &&
                    argument.type != KRATOS_TYPE_BOOL && argument.type != KRATOS_TYPE_CHAR) {
                    semantic_error(analyzer, node->line, "tipo non convertibile");
                }
                return strcmp(node->as.call_expr.callee, "to_int") == 0
                    ? type_info(KRATOS_TYPE_INT, 0) : type_info(KRATOS_TYPE_FLOAT, 0);
            }
            Symbol *symbol = scope_find(analyzer->scope, node->as.call_expr.callee);
            if (symbol == NULL || !symbol->is_function || symbol->func == NULL) {
                semantic_error(analyzer, node->line, "chiamata a una craft inesistente");
                return invalid;
            }

            AstNode *func = symbol->func;
            size_t expected = func->as.func_decl.params.count;
            size_t given = node->as.call_expr.arguments.count;
            if (expected != given) {
                semantic_error(analyzer, node->line, "numero di argomenti errato");
            }

            size_t n = expected < given ? expected : given;
            for (size_t i = 0; i < n; i++) {
                TypeInfo arg = check_expr(analyzer, node->as.call_expr.arguments.items[i]);
                TypeInfo param = type_info(func->as.func_decl.params.items[i]->as.param.type, 0);
                if (!assignable(arg, param)) {
                    semantic_error(analyzer, node->line, "tipo di argomento incompatibile");
                }
            }

            return type_info(func->as.func_decl.return_type, 0);
        }

        case AST_ARRAY_LITERAL: {
            TypeInfo element = type_info(KRATOS_TYPE_VOID, 0);
            int have_element = 0;
            for (size_t i = 0; i < node->as.array_literal.elements.count; i++) {
                TypeInfo item = check_expr(analyzer, node->as.array_literal.elements.items[i]);
                if (!have_element) {
                    element = item;
                    have_element = 1;
                } else if (!types_equal(item, element)) {
                    semantic_error(analyzer, node->line, "elementi dell'array di tipi diversi");
                }
            }
            if (!have_element) {
                return type_info(KRATOS_TYPE_VOID, 1);
            }
            return type_info(element.type, element.is_array + 1);
        }

        case AST_INDEX_EXPR: {
            TypeInfo array = check_expr(analyzer, node->as.index_expr.array);
            TypeInfo index = check_expr(analyzer, node->as.index_expr.index);
            if (array.is_array == 0) {
                semantic_error(analyzer, node->line, "indicizzazione di un valore che non e' un array");
            }
            if (index.is_array || index.type != KRATOS_TYPE_INT) {
                semantic_error(analyzer, node->line, "l'indice deve essere k_int");
            }
            return type_info(array.type, array.is_array > 0 ? array.is_array - 1 : 0);
        }

        default:
            semantic_error(analyzer, node->line, "espressione non valida");
            break;
    }

    return invalid;
}


static void check_var_decl(Analyzer *analyzer, AstNode *node)
{
    if (node->as.var_decl.type == KRATOS_TYPE_VOID && !node->as.var_decl.is_array) {
        semantic_error(analyzer, node->line, "k_void non puo' dichiarare una variabile");
    }

    TypeInfo declared = type_info(node->as.var_decl.type, node->as.var_decl.is_array);
    TypeInfo init = check_expr(analyzer, node->as.var_decl.initializer);

    if (init.is_array && init.type == KRATOS_TYPE_VOID && declared.is_array) {
        init = declared;
    }

    if (!assignable(init, declared)) {
        semantic_error(analyzer, node->line, "inizializzatore incompatibile con il tipo dichiarato");
    }

    scope_add(
        analyzer,
        node->line,
        node->as.var_decl.name,
        declared,
        node->as.var_decl.is_const,
        0,
        NULL
    );
}


static void check_stmt(Analyzer *analyzer, AstNode *node)
{
    if (node == NULL) {
        return;
    }

    switch (node->kind) {
        case AST_VAR_DECL:
            check_var_decl(analyzer, node);
            break;

        case AST_BLOCK: {
            Scope inner;
            scope_init(&inner, analyzer->scope);
            analyzer->scope = &inner;
            for (size_t i = 0; i < node->as.block.statements.count; i++) {
                check_stmt(analyzer, node->as.block.statements.items[i]);
            }
            analyzer->scope = inner.parent;
            scope_free(&inner);
            break;
        }

        case AST_IF_STMT: {
            for (size_t i = 0; i < node->as.if_stmt.branches.count; i++) {
                AstNode *branch = node->as.if_stmt.branches.items[i];
                TypeInfo cond = check_expr(analyzer, branch->as.cond_branch.condition);
                if (cond.type != KRATOS_TYPE_BOOL || cond.is_array) {
                    semantic_error(analyzer, branch->line, "la condizione deve essere k_bool");
                }
                check_stmt(analyzer, branch->as.cond_branch.body);
            }
            if (node->as.if_stmt.else_block != NULL) {
                check_stmt(analyzer, node->as.if_stmt.else_block);
            }
            break;
        }

        case AST_HOLD_STMT: {
            TypeInfo cond = check_expr(analyzer, node->as.hold_stmt.condition);
            if (cond.type != KRATOS_TYPE_BOOL || cond.is_array) {
                semantic_error(analyzer, node->line, "la condizione di hold deve essere k_bool");
            }
            analyzer->loop_depth++;
            check_stmt(analyzer, node->as.hold_stmt.body);
            analyzer->loop_depth--;
            break;
        }

        case AST_PRESS_STMT: {
            TypeInfo cond = check_expr(analyzer, node->as.press_stmt.condition);
            if (cond.type != KRATOS_TYPE_BOOL || cond.is_array) {
                semantic_error(analyzer, node->line, "la condizione di press deve essere k_bool");
            }
            analyzer->loop_depth++;
            check_stmt(analyzer, node->as.press_stmt.body);
            analyzer->loop_depth--;
            break;
        }

        case AST_DRIVE_STMT: {
            Scope inner;
            scope_init(&inner, analyzer->scope);
            analyzer->scope = &inner;
            check_var_decl(analyzer, node->as.drive_stmt.init);
            TypeInfo cond = check_expr(analyzer, node->as.drive_stmt.condition);
            if (cond.type != KRATOS_TYPE_BOOL || cond.is_array) {
                semantic_error(analyzer, node->line, "la condizione di drive deve essere k_bool");
            }
            analyzer->loop_depth++;
            check_stmt(analyzer, node->as.drive_stmt.body);
            if (node->as.drive_stmt.step->kind == AST_ASSIGN) {
                check_stmt(analyzer, node->as.drive_stmt.step);
            } else {
                check_expr(analyzer, node->as.drive_stmt.step);
            }
            analyzer->loop_depth--;
            analyzer->scope = inner.parent;
            scope_free(&inner);
            break;
        }

        case AST_SWEEP_STMT: {
            Symbol *collection = scope_find(analyzer->scope, node->as.sweep_stmt.collection_name);
            if (collection == NULL || collection->is_function || collection->type.is_array != 1) {
                semantic_error(analyzer, node->line, "sweep richiede un array");
            } else if (collection->type.type != node->as.sweep_stmt.element_type) {
                semantic_error(analyzer, node->line, "il tipo dell'elemento non coincide con l'array");
            }

            Scope inner;
            scope_init(&inner, analyzer->scope);
            analyzer->scope = &inner;
            scope_add(
                analyzer,
                node->line,
                node->as.sweep_stmt.element_name,
                type_info(node->as.sweep_stmt.element_type, 0),
                0,
                0,
                NULL
            );
            analyzer->loop_depth++;
            check_stmt(analyzer, node->as.sweep_stmt.body);
            analyzer->loop_depth--;
            analyzer->scope = inner.parent;
            scope_free(&inner);
            break;
        }

        case AST_SNAP_STMT:
        case AST_PUSH_STMT:
            if (analyzer->loop_depth == 0) {
                semantic_error(analyzer, node->line, "snap/push ammessi solo dentro un ciclo");
            }
            break;

        case AST_YIELD_STMT:
            if (!analyzer->in_function) {
                semantic_error(analyzer, node->line, "yield fuori da una craft");
                break;
            }
            if (node->as.yield_stmt.value == NULL) {
                if (analyzer->current_return != KRATOS_TYPE_VOID) {
                    semantic_error(analyzer, node->line, "yield senza valore in una craft che restituisce un tipo");
                }
            } else {
                TypeInfo value = check_expr(analyzer, node->as.yield_stmt.value);
                TypeInfo expected = type_info(analyzer->current_return, 0);
                if (analyzer->current_return == KRATOS_TYPE_VOID) {
                    semantic_error(analyzer, node->line, "una craft k_void non puo' fare yield di un valore");
                } else if (!assignable(value, expected)) {
                    semantic_error(analyzer, node->line, "tipo di yield incompatibile");
                }
            }
            break;

        case AST_SHOUT_STMT:
            check_expr(analyzer, node->as.shout_stmt.value);
            break;

        case AST_WIELD_STMT:
            semantic_error(analyzer, node->line, "wield residuo dopo l'espansione degli import");
            break;

        case AST_ASSIGN: {
            TypeInfo target;
            if (node->as.assign.target->kind == AST_IDENTIFIER_EXPR) {
                Symbol *symbol = scope_find(analyzer->scope, node->as.assign.target->as.identifier_expr.name);
                if (symbol == NULL || symbol->is_function) {
                    semantic_error(analyzer, node->line, "assegnamento a un nome non dichiarato");
                    break;
                }
                if (symbol->is_const) {
                    semantic_error(analyzer, node->line, "impossibile assegnare a k_const");
                }
                target = symbol->type;
            } else if (node->as.assign.target->kind == AST_INDEX_EXPR) {
                AstNode *array_expr = node->as.assign.target->as.index_expr.array;
                TypeInfo array = check_expr(analyzer, array_expr);
                TypeInfo index = check_expr(analyzer, node->as.assign.target->as.index_expr.index);
                if (!array.is_array) {
                    semantic_error(analyzer, node->line, "assegnamento indicizzato richiede un array");
                }
                if (index.is_array || index.type != KRATOS_TYPE_INT) {
                    semantic_error(analyzer, node->line, "l'indice deve essere k_int");
                }
                if (array_expr->kind == AST_IDENTIFIER_EXPR) {
                    Symbol *symbol = scope_find(analyzer->scope, array_expr->as.identifier_expr.name);
                    if (symbol != NULL && symbol->is_const) {
                        semantic_error(analyzer, node->line, "impossibile assegnare a k_const");
                    }
                }
                target = type_info(array.type, 0);
            } else {
                semantic_error(analyzer, node->line, "destinazione di assegnamento non valida");
                break;
            }
            TypeInfo value = check_expr(analyzer, node->as.assign.value);
            if (!assignable(value, target)) {
                semantic_error(analyzer, node->line, "tipo di assegnamento incompatibile");
            }
            break;
        }

        case AST_EXPR_STMT:
            check_expr(analyzer, node->as.expr_stmt.expression);
            break;

        default:
            semantic_error(analyzer, node->line, "istruzione non valida a questo livello");
            break;
    }
}


static void check_function(Analyzer *analyzer, AstNode *func)
{
    Scope inner;
    scope_init(&inner, analyzer->scope);
    analyzer->scope = &inner;
    analyzer->in_function = 1;
    analyzer->current_return = func->as.func_decl.return_type;

    for (size_t i = 0; i < func->as.func_decl.params.count; i++) {
        AstNode *param = func->as.func_decl.params.items[i];
        if (param->as.param.type == KRATOS_TYPE_VOID) {
            semantic_error(analyzer, param->line, "un parametro non puo' essere k_void");
        }
        scope_add(
            analyzer,
            param->line,
            param->as.param.name,
            type_info(param->as.param.type, 0),
            0,
            0,
            NULL
        );
    }

    check_stmt(analyzer, func->as.func_decl.body);

    if (func->as.func_decl.return_type != KRATOS_TYPE_VOID && !always_returns(func->as.func_decl.body)) {
        semantic_error(analyzer, func->line, "la craft deve fare yield su ogni cammino");
    }

    analyzer->in_function = 0;
    analyzer->current_return = KRATOS_TYPE_VOID;
    analyzer->scope = inner.parent;
    scope_free(&inner);
}


static int path_on_stack(Analyzer *analyzer, const char *path)
{
    for (size_t i = 0; i < analyzer->wield_stack_count; i++) {
        if (strcmp(analyzer->wield_stack[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}


static void stack_push(Analyzer *analyzer, const char *path)
{
    if (analyzer->wield_stack_count == analyzer->wield_stack_capacity) {
        size_t new_capacity = analyzer->wield_stack_capacity == 0 ? 4 : analyzer->wield_stack_capacity * 2;
        char **new_items = realloc(analyzer->wield_stack, new_capacity * sizeof(char *));
        if (new_items == NULL) {
            fprintf(stderr, "kratos: memoria esaurita\n");
            exit(EXIT_FAILURE);
        }
        analyzer->wield_stack = new_items;
        analyzer->wield_stack_capacity = new_capacity;
    }
    analyzer->wield_stack[analyzer->wield_stack_count++] = kratos_copy_string(path);
}


static void stack_pop(Analyzer *analyzer)
{
    if (analyzer->wield_stack_count == 0) {
        return;
    }
    analyzer->wield_stack_count--;
    free(analyzer->wield_stack[analyzer->wield_stack_count]);
}


static AstNode *parse_source_file(Analyzer *analyzer, const char *path)
{
    char *source = kratos_read_file(path);
    if (source == NULL) {
        return NULL;
    }

    Lexer lexer;
    Parser parser;
    lexer_init(&lexer, source);
    parser_init(&parser, &lexer);
    AstNode *program = parser_parse_program(&parser);
    if (parser.had_error) {
        analyzer->had_error = 1;
        ast_free(program);
        free(source);
        return NULL;
    }

    free(source);
    return program;
}


static void expand_wields(Analyzer *analyzer, AstNode *program, const char *from_path);


static void expand_wields(Analyzer *analyzer, AstNode *program, const char *from_path)
{
    AstNodeList old = program->as.program.declarations;
    ast_node_list_init(&program->as.program.declarations);

    char *from_dir = kratos_dirname_dup(from_path != NULL ? from_path : ".");

    for (size_t i = 0; i < old.count; i++) {
        AstNode *node = old.items[i];
        if (node->kind != AST_WIELD_STMT) {
            ast_node_list_push(&program->as.program.declarations, node);
            continue;
        }

        char *joined = kratos_join_path(from_dir, node->as.wield_stmt.path);
        char *resolved = kratos_realpath_dup(joined);
        if (resolved == NULL) {
            semantic_error(analyzer, node->line, "file di wield non trovato");
            free(joined);
            ast_free(node);
            continue;
        }

        if (path_on_stack(analyzer, resolved)) {
            semantic_error(analyzer, node->line, "wield ciclico");
            free(joined);
            free(resolved);
            ast_free(node);
            continue;
        }

        stack_push(analyzer, resolved);
        AstNode *imported = parse_source_file(analyzer, resolved);
        if (imported != NULL) {
            expand_wields(analyzer, imported, resolved);
            for (size_t j = 0; j < imported->as.program.declarations.count; j++) {
                ast_node_list_push(
                    &program->as.program.declarations,
                    imported->as.program.declarations.items[j]
                );
            }
            imported->as.program.declarations.count = 0;
            ast_free(imported);
        }
        stack_pop(analyzer);

        free(joined);
        free(resolved);
        ast_free(node);
    }

    free(old.items);
    free(from_dir);
}


int semantic_analyze(AstNode *program, const char *source_path)
{
    return semantic_analyze_with_context(program, source_path, NULL);
}

int semantic_analyze_with_context(
    AstNode *program,
    const char *source_path,
    const DiagContext *diag_context
)
{
    Analyzer analyzer;
    memset(&analyzer, 0, sizeof(analyzer));
    analyzer.diag_context = diag_context;

    char *resolved_source = NULL;
    if (source_path != NULL) {
        resolved_source = kratos_realpath_dup(source_path);
        if (resolved_source != NULL) {
            stack_push(&analyzer, resolved_source);
        }
    }

    expand_wields(&analyzer, program, source_path != NULL ? source_path : ".");

    Scope global;
    scope_init(&global, NULL);
    analyzer.scope = &global;

    for (size_t i = 0; i < program->as.program.declarations.count; i++) {
        AstNode *node = program->as.program.declarations.items[i];
        if (node->kind == AST_FUNC_DECL) {
            scope_add(
                &analyzer,
                node->line,
                node->as.func_decl.name,
                type_info(node->as.func_decl.return_type, 0),
                0,
                1,
                node
            );
        }
    }

    for (size_t i = 0; i < program->as.program.declarations.count; i++) {
        AstNode *node = program->as.program.declarations.items[i];
        if (node->kind == AST_VAR_DECL) {
            check_var_decl(&analyzer, node);
        } else if (node->kind == AST_FUNC_DECL) {
            check_function(&analyzer, node);
        } else {
            semantic_error(&analyzer, node->line, "dichiarazione di primo livello non valida");
        }
    }

    scope_free(&global);
    while (analyzer.wield_stack_count > 0) {
        stack_pop(&analyzer);
    }
    free(analyzer.wield_stack);
    free(resolved_source);

    return analyzer.had_error ? 1 : 0;
}
