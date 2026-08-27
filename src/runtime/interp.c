#include "runtime/interp.h"

#include "utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_CHAR,
    VAL_STRING,
    VAL_VOID,
    VAL_ARRAY
} ValueKind;

typedef struct Value Value;

struct Value {
    ValueKind kind;
    KratosType element_type;
    union {
        int64_t i;
        double f;
        int b;
        char c;
        char *s;
        struct {
            Value *items;
            size_t count;
        } array;
    } as;
};

typedef enum {
    FLOW_OK,
    FLOW_SNAP,
    FLOW_PUSH,
    FLOW_YIELD,
    FLOW_ERROR
} Flow;

typedef struct Binding {
    char *name;
    int is_const;
    Value value;
    struct Binding *next;
} Binding;

typedef struct Env {
    Binding *bindings;
    struct Env *parent;
} Env;

typedef struct {
    char *name;
    AstNode *func;
} FuncEntry;

typedef struct {
    int had_error;
    Env *env;
    FuncEntry *functions;
    size_t function_count;
    size_t function_capacity;
    Value yield_value;
} Interp;


static Value val_void(void)
{
    Value v;
    memset(&v, 0, sizeof(v));
    v.kind = VAL_VOID;
    return v;
}


static Value val_int(int64_t n)
{
    Value v = val_void();
    v.kind = VAL_INT;
    v.as.i = n;
    return v;
}


static Value val_float(double n)
{
    Value v = val_void();
    v.kind = VAL_FLOAT;
    v.as.f = n;
    return v;
}


static Value val_bool(int b)
{
    Value v = val_void();
    v.kind = VAL_BOOL;
    v.as.b = b ? 1 : 0;
    return v;
}


static void value_free(Value value)
{
    if (value.kind == VAL_STRING) {
        free(value.as.s);
    } else if (value.kind == VAL_ARRAY) {
        for (size_t i = 0; i < value.as.array.count; i++) {
            value_free(value.as.array.items[i]);
        }
        free(value.as.array.items);
    }
}


static Value value_clone(Value value)
{
    if (value.kind == VAL_STRING) {
        Value copy = value;
        copy.as.s = kratos_copy_string(value.as.s != NULL ? value.as.s : "");
        return copy;
    }

    if (value.kind == VAL_ARRAY) {
        Value copy = value;
        copy.as.array.items = calloc(value.as.array.count, sizeof(Value));
        if (copy.as.array.items == NULL && value.as.array.count > 0) {
            fprintf(stderr, "kratos: memoria esaurita\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < value.as.array.count; i++) {
            copy.as.array.items[i] = value_clone(value.as.array.items[i]);
        }
        return copy;
    }

    return value;
}


static void runtime_error(Interp *interp, size_t line, const char *message)
{
    fprintf(stderr, "kratos: errore di runtime alla riga %zu: %s\n", line, message);
    interp->had_error = 1;
}


static Binding *env_find(Env *env, const char *name)
{
    for (Env *current = env; current != NULL; current = current->parent) {
        for (Binding *b = current->bindings; b != NULL; b = b->next) {
            if (strcmp(b->name, name) == 0) {
                return b;
            }
        }
    }
    return NULL;
}


static void env_define(Env *env, const char *name, int is_const, Value value)
{
    Binding *binding = malloc(sizeof(Binding));
    if (binding == NULL) {
        fprintf(stderr, "kratos: memoria esaurita\n");
        exit(EXIT_FAILURE);
    }
    binding->name = kratos_copy_string(name);
    binding->is_const = is_const;
    binding->value = value;
    binding->next = env->bindings;
    env->bindings = binding;
}


static void env_free_bindings(Env *env)
{
    Binding *b = env->bindings;
    while (b != NULL) {
        Binding *next = b->next;
        free(b->name);
        value_free(b->value);
        free(b);
        b = next;
    }
    env->bindings = NULL;
}


static AstNode *find_function(Interp *interp, const char *name)
{
    for (size_t i = 0; i < interp->function_count; i++) {
        if (strcmp(interp->functions[i].name, name) == 0) {
            return interp->functions[i].func;
        }
    }
    return NULL;
}


static void register_function(Interp *interp, AstNode *func)
{
    if (interp->function_count == interp->function_capacity) {
        size_t new_capacity = interp->function_capacity == 0 ? 8 : interp->function_capacity * 2;
        FuncEntry *new_items = realloc(interp->functions, new_capacity * sizeof(FuncEntry));
        if (new_items == NULL) {
            fprintf(stderr, "kratos: memoria esaurita\n");
            exit(EXIT_FAILURE);
        }
        interp->functions = new_items;
        interp->function_capacity = new_capacity;
    }
    interp->functions[interp->function_count].name = func->as.func_decl.name;
    interp->functions[interp->function_count].func = func;
    interp->function_count++;
}


static Value eval_expr(Interp *interp, AstNode *node);
static Flow eval_stmt(Interp *interp, AstNode *node);


static int is_truthy(Value value)
{
    return value.kind == VAL_BOOL && value.as.b;
}


static Value numeric_promote_left(Value left, Value right)
{
    if (left.kind == VAL_INT && right.kind == VAL_FLOAT) {
        return val_float((double)left.as.i);
    }
    return left;
}


static void shout_value(Value value)
{
    switch (value.kind) {
        case VAL_INT:
            printf("%lld\n", (long long)value.as.i);
            break;
        case VAL_FLOAT:
            printf("%g\n", value.as.f);
            break;
        case VAL_BOOL:
            printf("%s\n", value.as.b ? "true" : "false");
            break;
        case VAL_CHAR:
            printf("%c\n", value.as.c);
            break;
        case VAL_STRING:
            printf("%s\n", value.as.s != NULL ? value.as.s : "");
            break;
        case VAL_VOID:
            printf("\n");
            break;
        case VAL_ARRAY:
            printf("[");
            for (size_t i = 0; i < value.as.array.count; i++) {
                if (i > 0) {
                    printf(", ");
                }
                switch (value.as.array.items[i].kind) {
                    case VAL_INT:
                        printf("%lld", (long long)value.as.array.items[i].as.i);
                        break;
                    case VAL_FLOAT:
                        printf("%g", value.as.array.items[i].as.f);
                        break;
                    case VAL_BOOL:
                        printf("%s", value.as.array.items[i].as.b ? "true" : "false");
                        break;
                    case VAL_CHAR:
                        printf("'%c'", value.as.array.items[i].as.c);
                        break;
                    case VAL_STRING:
                        printf("\"%s\"", value.as.array.items[i].as.s != NULL ? value.as.array.items[i].as.s : "");
                        break;
                    default:
                        printf("?");
                        break;
                }
            }
            printf("]\n");
            break;
    }
}


static Value call_function(Interp *interp, AstNode *func, AstNodeList *args, size_t line)
{
    Env frame;
    frame.bindings = NULL;
    frame.parent = interp->env;

    size_t n = args->count;
    if (n != func->as.func_decl.params.count) {
        runtime_error(interp, line, "numero di argomenti errato");
        return val_void();
    }

    for (size_t i = 0; i < n; i++) {
        Value arg = eval_expr(interp, args->items[i]);
        if (interp->had_error) {
            value_free(arg);
            env_free_bindings(&frame);
            return val_void();
        }
        env_define(&frame, func->as.func_decl.params.items[i]->as.param.name, 0, arg);
    }

    Env *previous = interp->env;
    interp->env = &frame;
    Flow flow = eval_stmt(interp, func->as.func_decl.body);
    interp->env = previous;

    Value result = val_void();
    if (flow == FLOW_YIELD) {
        result = interp->yield_value;
        interp->yield_value = val_void();
    }

    env_free_bindings(&frame);
    return result;
}


static Value eval_expr(Interp *interp, AstNode *node)
{
    if (interp->had_error || node == NULL) {
        return val_void();
    }

    switch (node->kind) {
        case AST_LITERAL_EXPR:
            switch (node->as.literal_expr.kind) {
                case LITERAL_INT:
                    return val_int(node->as.literal_expr.value.int_value);
                case LITERAL_FLOAT:
                    return val_float(node->as.literal_expr.value.float_value);
                case LITERAL_BOOL:
                    return val_bool(node->as.literal_expr.value.bool_value);
                case LITERAL_CHAR: {
                    Value v = val_void();
                    v.kind = VAL_CHAR;
                    v.as.c = node->as.literal_expr.value.char_value;
                    return v;
                }
                case LITERAL_STRING: {
                    Value v = val_void();
                    v.kind = VAL_STRING;
                    v.as.s = kratos_copy_string(
                        node->as.literal_expr.value.string_value != NULL
                            ? node->as.literal_expr.value.string_value
                            : ""
                    );
                    return v;
                }
            }
            break;

        case AST_IDENTIFIER_EXPR: {
            Binding *binding = env_find(interp->env, node->as.identifier_expr.name);
            if (binding == NULL) {
                runtime_error(interp, node->line, "variabile non trovata");
                return val_void();
            }
            return value_clone(binding->value);
        }

        case AST_UNARY_EXPR: {
            Value operand = eval_expr(interp, node->as.unary_expr.operand);
            if (node->as.unary_expr.operator == TOKEN_NOT) {
                int result = !is_truthy(operand);
                value_free(operand);
                return val_bool(result);
            }
            if (operand.kind == VAL_INT) {
                int64_t n = -operand.as.i;
                value_free(operand);
                return val_int(n);
            }
            if (operand.kind == VAL_FLOAT) {
                double n = -operand.as.f;
                value_free(operand);
                return val_float(n);
            }
            value_free(operand);
            runtime_error(interp, node->line, "negazione non numerica");
            return val_void();
        }

        case AST_BINARY_EXPR: {
            TokenType op = node->as.binary_expr.operator;
            Value left = eval_expr(interp, node->as.binary_expr.left);
            if (interp->had_error) {
                value_free(left);
                return val_void();
            }

            if (op == TOKEN_AND) {
                if (!is_truthy(left)) {
                    value_free(left);
                    return val_bool(0);
                }
                value_free(left);
                Value right = eval_expr(interp, node->as.binary_expr.right);
                int t = is_truthy(right);
                value_free(right);
                return val_bool(t);
            }

            if (op == TOKEN_OR) {
                if (is_truthy(left)) {
                    value_free(left);
                    return val_bool(1);
                }
                value_free(left);
                Value right = eval_expr(interp, node->as.binary_expr.right);
                int t = is_truthy(right);
                value_free(right);
                return val_bool(t);
            }

            Value right = eval_expr(interp, node->as.binary_expr.right);
            if (interp->had_error) {
                value_free(left);
                value_free(right);
                return val_void();
            }

            if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL) {
                int eq = 0;
                if (left.kind == VAL_INT && right.kind == VAL_INT) {
                    eq = left.as.i == right.as.i;
                } else if (left.kind == VAL_FLOAT && right.kind == VAL_FLOAT) {
                    eq = left.as.f == right.as.f;
                } else if (left.kind == VAL_BOOL && right.kind == VAL_BOOL) {
                    eq = left.as.b == right.as.b;
                } else if (left.kind == VAL_CHAR && right.kind == VAL_CHAR) {
                    eq = left.as.c == right.as.c;
                } else if (left.kind == VAL_STRING && right.kind == VAL_STRING) {
                    eq = strcmp(left.as.s != NULL ? left.as.s : "", right.as.s != NULL ? right.as.s : "") == 0;
                }
                value_free(left);
                value_free(right);
                return val_bool(op == TOKEN_EQUAL ? eq : !eq);
            }

            Value a = numeric_promote_left(left, right);
            Value b = numeric_promote_left(right, left);

            if (op == TOKEN_LESS || op == TOKEN_GREATER || op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {
                int cmp;
                if (a.kind == VAL_FLOAT || b.kind == VAL_FLOAT) {
                    double x = a.kind == VAL_FLOAT ? a.as.f : (double)a.as.i;
                    double y = b.kind == VAL_FLOAT ? b.as.f : (double)b.as.i;
                    cmp = (x > y) - (x < y);
                } else {
                    cmp = (a.as.i > b.as.i) - (a.as.i < b.as.i);
                }
                value_free(left);
                value_free(right);
                int result = 0;
                if (op == TOKEN_LESS) result = cmp < 0;
                else if (op == TOKEN_GREATER) result = cmp > 0;
                else if (op == TOKEN_LESS_EQUAL) result = cmp <= 0;
                else result = cmp >= 0;
                return val_bool(result);
            }

            if (a.kind == VAL_FLOAT || b.kind == VAL_FLOAT) {
                double x = a.kind == VAL_FLOAT ? a.as.f : (double)a.as.i;
                double y = b.kind == VAL_FLOAT ? b.as.f : (double)b.as.i;
                double r = 0.0;
                if (op == TOKEN_PLUS) r = x + y;
                else if (op == TOKEN_MINUS) r = x - y;
                else if (op == TOKEN_STAR) r = x * y;
                else if (op == TOKEN_SLASH) {
                    if (y == 0.0) {
                        runtime_error(interp, node->line, "divisione per zero");
                    } else {
                        r = x / y;
                    }
                }
                value_free(left);
                value_free(right);
                return val_float(r);
            }

            if (op == TOKEN_SLASH && b.as.i == 0) {
                runtime_error(interp, node->line, "divisione per zero");
                value_free(left);
                value_free(right);
                return val_int(0);
            }
            if (op == TOKEN_PERCENT && b.as.i == 0) {
                runtime_error(interp, node->line, "modulo per zero");
                value_free(left);
                value_free(right);
                return val_int(0);
            }

            int64_t r = 0;
            if (op == TOKEN_PLUS) r = a.as.i + b.as.i;
            else if (op == TOKEN_MINUS) r = a.as.i - b.as.i;
            else if (op == TOKEN_STAR) r = a.as.i * b.as.i;
            else if (op == TOKEN_SLASH) r = a.as.i / b.as.i;
            else if (op == TOKEN_PERCENT) r = a.as.i % b.as.i;

            value_free(left);
            value_free(right);
            return val_int(r);
        }

        case AST_CALL_EXPR: {
            AstNode *func = find_function(interp, node->as.call_expr.callee);
            if (func == NULL) {
                runtime_error(interp, node->line, "craft non trovata");
                return val_void();
            }
            return call_function(interp, func, &node->as.call_expr.arguments, node->line);
        }

        case AST_ARRAY_LITERAL: {
            Value array = val_void();
            array.kind = VAL_ARRAY;
            array.as.array.count = node->as.array_literal.elements.count;
            array.as.array.items = calloc(array.as.array.count, sizeof(Value));
            if (array.as.array.items == NULL && array.as.array.count > 0) {
                fprintf(stderr, "kratos: memoria esaurita\n");
                exit(EXIT_FAILURE);
            }
            for (size_t i = 0; i < array.as.array.count; i++) {
                array.as.array.items[i] = eval_expr(interp, node->as.array_literal.elements.items[i]);
                if (i == 0) {
                    switch (array.as.array.items[i].kind) {
                        case VAL_INT:    array.element_type = KRATOS_TYPE_INT; break;
                        case VAL_FLOAT:  array.element_type = KRATOS_TYPE_FLOAT; break;
                        case VAL_BOOL:   array.element_type = KRATOS_TYPE_BOOL; break;
                        case VAL_CHAR:   array.element_type = KRATOS_TYPE_CHAR; break;
                        case VAL_STRING: array.element_type = KRATOS_TYPE_STRING; break;
                        default:         array.element_type = KRATOS_TYPE_VOID; break;
                    }
                }
            }
            return array;
        }

        case AST_INDEX_EXPR: {
            Value array = eval_expr(interp, node->as.index_expr.array);
            Value index = eval_expr(interp, node->as.index_expr.index);
            Value result = val_void();
            if (array.kind != VAL_ARRAY || index.kind != VAL_INT) {
                runtime_error(interp, node->line, "indicizzazione non valida");
            } else if (index.as.i < 0 || (size_t)index.as.i >= array.as.array.count) {
                runtime_error(interp, node->line, "indice fuori dai limiti");
            } else {
                result = value_clone(array.as.array.items[index.as.i]);
            }
            value_free(array);
            value_free(index);
            return result;
        }

        default:
            runtime_error(interp, node->line, "espressione non eseguibile");
            break;
    }

    return val_void();
}


static Flow eval_block(Interp *interp, AstNode *block)
{
    Env inner;
    inner.bindings = NULL;
    inner.parent = interp->env;
    interp->env = &inner;

    Flow flow = FLOW_OK;
    for (size_t i = 0; i < block->as.block.statements.count; i++) {
        flow = eval_stmt(interp, block->as.block.statements.items[i]);
        if (flow != FLOW_OK || interp->had_error) {
            break;
        }
    }

    interp->env = inner.parent;
    env_free_bindings(&inner);
    return flow;
}


static Flow eval_stmt(Interp *interp, AstNode *node)
{
    if (interp->had_error || node == NULL) {
        return FLOW_ERROR;
    }

    switch (node->kind) {
        case AST_VAR_DECL: {
            Value init = eval_expr(interp, node->as.var_decl.initializer);
            env_define(interp->env, node->as.var_decl.name, node->as.var_decl.is_const, init);
            return FLOW_OK;
        }

        case AST_BLOCK:
            return eval_block(interp, node);

        case AST_IF_STMT: {
            for (size_t i = 0; i < node->as.if_stmt.branches.count; i++) {
                AstNode *branch = node->as.if_stmt.branches.items[i];
                Value cond = eval_expr(interp, branch->as.cond_branch.condition);
                int t = is_truthy(cond);
                value_free(cond);
                if (t) {
                    return eval_stmt(interp, branch->as.cond_branch.body);
                }
            }
            if (node->as.if_stmt.else_block != NULL) {
                return eval_stmt(interp, node->as.if_stmt.else_block);
            }
            return FLOW_OK;
        }

        case AST_HOLD_STMT:
            while (1) {
                Value cond = eval_expr(interp, node->as.hold_stmt.condition);
                int t = is_truthy(cond);
                value_free(cond);
                if (!t || interp->had_error) {
                    break;
                }
                Flow flow = eval_stmt(interp, node->as.hold_stmt.body);
                if (flow == FLOW_SNAP) {
                    break;
                }
                if (flow == FLOW_PUSH) {
                    continue;
                }
                if (flow != FLOW_OK) {
                    return flow;
                }
            }
            return FLOW_OK;

        case AST_PRESS_STMT:
            while (1) {
                Flow flow = eval_stmt(interp, node->as.press_stmt.body);
                if (flow == FLOW_SNAP) {
                    break;
                }
                if (flow != FLOW_OK && flow != FLOW_PUSH) {
                    return flow;
                }
                Value cond = eval_expr(interp, node->as.press_stmt.condition);
                int t = is_truthy(cond);
                value_free(cond);
                if (!t || interp->had_error) {
                    break;
                }
            }
            return FLOW_OK;

        case AST_DRIVE_STMT: {
            Env inner;
            inner.bindings = NULL;
            inner.parent = interp->env;
            interp->env = &inner;

            Flow flow = eval_stmt(interp, node->as.drive_stmt.init);
            if (flow == FLOW_OK && !interp->had_error) {
                while (1) {
                    Value cond = eval_expr(interp, node->as.drive_stmt.condition);
                    int t = is_truthy(cond);
                    value_free(cond);
                    if (!t || interp->had_error) {
                        break;
                    }
                    flow = eval_stmt(interp, node->as.drive_stmt.body);
                    if (flow == FLOW_SNAP) {
                        flow = FLOW_OK;
                        break;
                    }
                    if (flow != FLOW_OK && flow != FLOW_PUSH) {
                        break;
                    }
                    if (node->as.drive_stmt.step->kind == AST_ASSIGN) {
                        Flow step_flow = eval_stmt(interp, node->as.drive_stmt.step);
                        if (step_flow != FLOW_OK) {
                            flow = step_flow;
                            break;
                        }
                    } else {
                        Value step = eval_expr(interp, node->as.drive_stmt.step);
                        value_free(step);
                    }
                    flow = FLOW_OK;
                }
            }

            interp->env = inner.parent;
            env_free_bindings(&inner);
            return flow;
        }

        case AST_SWEEP_STMT: {
            Binding *collection = env_find(interp->env, node->as.sweep_stmt.collection_name);
            if (collection == NULL || collection->value.kind != VAL_ARRAY) {
                runtime_error(interp, node->line, "sweep su un valore che non e' un array");
                return FLOW_ERROR;
            }

            Env inner;
            inner.bindings = NULL;
            inner.parent = interp->env;
            interp->env = &inner;
            env_define(&inner, node->as.sweep_stmt.element_name, 0, val_void());
            Binding *element = inner.bindings;

            Flow flow = FLOW_OK;
            for (size_t i = 0; i < collection->value.as.array.count; i++) {
                value_free(element->value);
                element->value = value_clone(collection->value.as.array.items[i]);
                flow = eval_stmt(interp, node->as.sweep_stmt.body);
                if (flow == FLOW_SNAP) {
                    flow = FLOW_OK;
                    break;
                }
                if (flow == FLOW_PUSH) {
                    flow = FLOW_OK;
                    continue;
                }
                if (flow != FLOW_OK) {
                    break;
                }
            }

            interp->env = inner.parent;
            env_free_bindings(&inner);
            return flow;
        }

        case AST_SNAP_STMT:
            return FLOW_SNAP;

        case AST_PUSH_STMT:
            return FLOW_PUSH;

        case AST_YIELD_STMT:
            if (node->as.yield_stmt.value != NULL) {
                value_free(interp->yield_value);
                interp->yield_value = eval_expr(interp, node->as.yield_stmt.value);
            } else {
                value_free(interp->yield_value);
                interp->yield_value = val_void();
            }
            return FLOW_YIELD;

        case AST_SHOUT_STMT: {
            Value value = eval_expr(interp, node->as.shout_stmt.value);
            if (!interp->had_error) {
                shout_value(value);
            }
            value_free(value);
            return FLOW_OK;
        }

        case AST_ASSIGN: {
            Binding *binding = env_find(interp->env, node->as.assign.target);
            if (binding == NULL) {
                runtime_error(interp, node->line, "assegnamento a variabile inesistente");
                return FLOW_ERROR;
            }
            Value value = eval_expr(interp, node->as.assign.value);
            value_free(binding->value);
            binding->value = value;
            return FLOW_OK;
        }

        case AST_EXPR_STMT: {
            Value value = eval_expr(interp, node->as.expr_stmt.expression);
            value_free(value);
            return FLOW_OK;
        }

        default:
            runtime_error(interp, node->line, "istruzione non eseguibile");
            return FLOW_ERROR;
    }
}


int interp_run(AstNode *program)
{
    Interp interp;
    memset(&interp, 0, sizeof(interp));
    interp.yield_value = val_void();

    Env global;
    global.bindings = NULL;
    global.parent = NULL;
    interp.env = &global;

    for (size_t i = 0; i < program->as.program.declarations.count; i++) {
        AstNode *node = program->as.program.declarations.items[i];
        if (node->kind == AST_FUNC_DECL) {
            register_function(&interp, node);
        }
    }

    for (size_t i = 0; i < program->as.program.declarations.count; i++) {
        AstNode *node = program->as.program.declarations.items[i];
        if (node->kind == AST_VAR_DECL) {
            eval_stmt(&interp, node);
            if (interp.had_error) {
                break;
            }
        }
    }

    if (!interp.had_error) {
        AstNode *main_fn = find_function(&interp, "main");
        if (main_fn != NULL) {
            AstNodeList no_args;
            ast_node_list_init(&no_args);
            Value result = call_function(&interp, main_fn, &no_args, main_fn->line);
            value_free(result);
        }
    }

    env_free_bindings(&global);
    value_free(interp.yield_value);
    free(interp.functions);
    return interp.had_error ? 1 : 0;
}
