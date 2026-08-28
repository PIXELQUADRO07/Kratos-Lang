#ifndef KRATOS_AST_H
#define KRATOS_AST_H

#include <stddef.h>
#include <stdint.h>

#include "lexer/lexer.h"

/*
 * Tipi di dato di Kratos come li vede l'AST (indipendenti dai token del
 * lexer). k_void e' riconosciuto dal lexer (TOKEN_K_VOID) e, a livello
 * semantico, e' ammesso solo come tipo di ritorno di una "craft".
 */
typedef enum {
    KRATOS_TYPE_INT,
    KRATOS_TYPE_FLOAT,
    KRATOS_TYPE_BOOL,
    KRATOS_TYPE_CHAR,
    KRATOS_TYPE_STRING,
    KRATOS_TYPE_VOID
} KratosType;


/*
 * Ogni possibile nodo dell'AST. I commenti indicano a quale regola della
 * grammatica (docs/specification.md) corrisponde.
 */
typedef enum {
    /* program = { declaration | function } ; */
    AST_PROGRAM,

    /* declaration = [ "k_const" ] type identifier "=" expression ";" ; */
    AST_VAR_DECL,

    /* function = return_type "craft" identifier "(" [ params ] ")" block ; */
    AST_FUNC_DECL,

    /* param = type identifier ; (un singolo parametro dentro AST_FUNC_DECL) */
    AST_PARAM,

    /* block = "{" { statement } "}" ; */
    AST_BLOCK,

    /*
     * if_stmt = "if" "(" expression ")" block
     *           { "elif" "(" expression ")" block }
     *           [ "else" block ] ;
     *
     * Il primo "if" e ogni "elif" successivo sono rappresentati come una
     * lista di AST_COND_BRANCH dentro AST_IF_STMT; "else" e' un blocco
     * opzionale a parte, senza condizione.
     */
    AST_IF_STMT,
    AST_COND_BRANCH,

    /* hold_stmt = "hold" "(" expression ")" block ; */
    AST_HOLD_STMT,

    /* press_stmt = "press" block "hold" "(" expression ")" ";" ; */
    AST_PRESS_STMT,

    /* drive_stmt = "drive" "(" declaration expression ";" expression ")" block ; */
    AST_DRIVE_STMT,

    /* sweep_stmt = "sweep" "(" type identifier "in" identifier ")" block ; */
    AST_SWEEP_STMT,

    /* "snap" ";" */
    AST_SNAP_STMT,

    /* "push" ";" */
    AST_PUSH_STMT,

    /* "yield" [ expression ] ";" */
    AST_YIELD_STMT,

    /* "shout" "(" expression ")" ";" */
    AST_SHOUT_STMT,

    /* "wield" string ";" */
    AST_WIELD_STMT,

    /*
     * assignment = identifier "=" expression ";" ;
     * Stesso nodo e' riusato, senza il ";", come clausola di step di
     * AST_DRIVE_STMT ("i = i + 1").
     */
    AST_ASSIGN,

    /* expr_stmt = expression ";" ; */
    AST_EXPR_STMT,

    /* expression = expression operator expression ; */
    AST_BINARY_EXPR,

    /* identifier (uso come espressione, es. dentro una condizione) */
    AST_IDENTIFIER_EXPR,

    /* literal = integer | float | boolean | character | string ; */
    AST_LITERAL_EXPR,

    /* "[" [ expression { "," expression } ] "]" : letterale di array, es. [1, 2, 3] */
    AST_ARRAY_LITERAL,

    /* identifier "[" expression "]" : accesso a un elemento, es. Lista[0] */
    AST_INDEX_EXPR,

    /* ("not" | "-") expression : negazione logica o aritmetica */
    AST_UNARY_EXPR,

    /*
     * Non ancora presente nella grammatica formale, ma la sintassi di
     * chiamata e' gia' decisa in docs/functions.md ("add(1, 2)"): riservato
     * per quando la produzione verra' aggiunta alla grammatica.
     */
    AST_CALL_EXPR
} AstNodeKind;


/*
 * Tipo di letterale contenuto in un nodo AST_LITERAL_EXPR.
 */
typedef enum {
    LITERAL_INT,
    LITERAL_FLOAT,
    LITERAL_BOOL,
    LITERAL_CHAR,
    LITERAL_STRING
} AstLiteralKind;


typedef struct AstNode AstNode;

/*
 * Lista dinamica di nodi, usata ovunque la grammatica ha una ripetizione:
 * il corpo di un blocco, i parametri di una funzione, le dichiarazioni di
 * un programma, i rami if/elif.
 */
typedef struct {
    AstNode **items;
    size_t count;
    size_t capacity;
} AstNodeList;

void ast_node_list_init(AstNodeList *list);
void ast_node_list_push(AstNodeList *list, AstNode *node);
void ast_node_list_free(AstNodeList *list);


struct AstNode {
    AstNodeKind kind;

    /* Span del token principale del nodo, per i messaggi diagnostici. */
    size_t line;
    size_t column;
    size_t length;

    union {
        /* AST_PROGRAM */
        struct {
            AstNodeList declarations;
        } program;

        /* AST_VAR_DECL */
        struct {
            int is_const;
            int is_array; /* 1 per "k_type[] nome = ...;" */
            KratosType type;
            char *name;
            AstNode *initializer; /* sempre presente: la grammatica richiede "= expression" */
        } var_decl;

        /* AST_FUNC_DECL */
        struct {
            KratosType return_type;
            char *name;
            AstNodeList params; /* elementi di kind AST_PARAM */
            AstNode *body;      /* AST_BLOCK */
        } func_decl;

        /* AST_PARAM */
        struct {
            KratosType type;
            char *name;
        } param;

        /* AST_BLOCK */
        struct {
            AstNodeList statements;
        } block;

        /* AST_IF_STMT */
        struct {
            AstNodeList branches; /* AST_COND_BRANCH: il primo e' l'"if", gli altri gli "elif" */
            AstNode *else_block;  /* AST_BLOCK oppure NULL se non c'e' "else" */
        } if_stmt;

        /* AST_COND_BRANCH */
        struct {
            AstNode *condition;
            AstNode *body; /* AST_BLOCK */
        } cond_branch;

        /* AST_HOLD_STMT */
        struct {
            AstNode *condition;
            AstNode *body; /* AST_BLOCK */
        } hold_stmt;

        /* AST_PRESS_STMT */
        struct {
            AstNode *body;      /* AST_BLOCK, eseguito prima della condizione */
            AstNode *condition;
        } press_stmt;

        /* AST_DRIVE_STMT */
        struct {
            AstNode *init;      /* AST_VAR_DECL */
            AstNode *condition; /* espressione booleana */
            AstNode *step;      /* tipicamente AST_ASSIGN */
            AstNode *body;      /* AST_BLOCK */
        } drive_stmt;

        /* AST_SWEEP_STMT */
        struct {
            KratosType element_type;
            char *element_name;
            char *collection_name;
            AstNode *body; /* AST_BLOCK */
        } sweep_stmt;

        /* AST_YIELD_STMT */
        struct {
            AstNode *value; /* espressione, o NULL per "yield;" in una k_void craft */
        } yield_stmt;

        /* AST_SHOUT_STMT */
        struct {
            AstNode *value;
        } shout_stmt;

        /* AST_WIELD_STMT */
        struct {
            char *path;
        } wield_stmt;

        /* AST_ASSIGN */
        struct {
            AstNode *target;
            AstNode *value;
        } assign;

        /* AST_EXPR_STMT */
        struct {
            AstNode *expression;
        } expr_stmt;

        /* AST_BINARY_EXPR */
        struct {
            TokenType operator; /* es. TOKEN_PLUS, TOKEN_LESS, TOKEN_AND ... */
            AstNode *left;
            AstNode *right;
        } binary_expr;

        /* AST_IDENTIFIER_EXPR */
        struct {
            char *name;
        } identifier_expr;

        /* AST_LITERAL_EXPR */
        struct {
            AstLiteralKind kind;
            union {
                int64_t int_value;
                double float_value;
                int bool_value;   /* 0 o 1 */
                char char_value;
                char *string_value;
            } value;
        } literal_expr;

        /* AST_CALL_EXPR */
        struct {
            char *callee;
            AstNodeList arguments;
        } call_expr;

        /* AST_ARRAY_LITERAL */
        struct {
            AstNodeList elements;
        } array_literal;

        /* AST_INDEX_EXPR */
        struct {
            AstNode *array; /* tipicamente AST_IDENTIFIER_EXPR */
            AstNode *index;
        } index_expr;

        /* AST_UNARY_EXPR */
        struct {
            TokenType operator; /* TOKEN_NOT o TOKEN_MINUS */
            AstNode *operand;
        } unary_expr;
    } as;
};

/* --- Costruttori: allocano il nodo e ne impostano kind + line. --- */

AstNode *ast_new_program(size_t line);
AstNode *ast_new_var_decl(size_t line, int is_const, int is_array, KratosType type, const char *name, AstNode *initializer);
AstNode *ast_new_func_decl(size_t line, KratosType return_type, const char *name, AstNode *body);
AstNode *ast_new_param(size_t line, KratosType type, const char *name);
AstNode *ast_new_block(size_t line);
AstNode *ast_new_if_stmt(size_t line);
AstNode *ast_new_cond_branch(size_t line, AstNode *condition, AstNode *body);
AstNode *ast_new_hold_stmt(size_t line, AstNode *condition, AstNode *body);
AstNode *ast_new_press_stmt(size_t line, AstNode *body, AstNode *condition);
AstNode *ast_new_drive_stmt(size_t line, AstNode *init, AstNode *condition, AstNode *step, AstNode *body);
AstNode *ast_new_sweep_stmt(size_t line, KratosType element_type, const char *element_name, const char *collection_name, AstNode *body);
AstNode *ast_new_snap_stmt(size_t line);
AstNode *ast_new_push_stmt(size_t line);
AstNode *ast_new_yield_stmt(size_t line, AstNode *value);
AstNode *ast_new_shout_stmt(size_t line, AstNode *value);
AstNode *ast_new_wield_stmt(size_t line, const char *path);
AstNode *ast_new_assign(size_t line, AstNode *target, AstNode *value);
AstNode *ast_new_expr_stmt(size_t line, AstNode *expression);
AstNode *ast_new_binary_expr(size_t line, TokenType operator, AstNode *left, AstNode *right);
AstNode *ast_new_identifier_expr(size_t line, const char *name);
AstNode *ast_new_literal_int(size_t line, int64_t value);
AstNode *ast_new_literal_float(size_t line, double value);
AstNode *ast_new_literal_bool(size_t line, int value);
AstNode *ast_new_literal_char(size_t line, char value);
AstNode *ast_new_literal_string(size_t line, const char *value);
AstNode *ast_new_call_expr(size_t line, const char *callee);
AstNode *ast_new_array_literal(size_t line);
AstNode *ast_new_index_expr(size_t line, AstNode *array, AstNode *index);
AstNode *ast_new_unary_expr(size_t line, TokenType operator, AstNode *operand);

/*
 * Libera ricorsivamente un nodo e tutti i suoi figli.
 * Accetta NULL senza fare nulla.
 */
void ast_free(AstNode *node);

/*
 * Nome leggibile di un KratosType, per messaggi diagnostici e stampa.
 */
const char *kratos_type_name(KratosType type);

/*
 * Stampa l'albero su stdout, indentato, per debug manuale in assenza di un
 * parser: utile finche' non esiste ancora un semantic analyzer.
 */
void ast_print(const AstNode *node, int indent);

void ast_set_span(AstNode *node, size_t line, size_t column, size_t length);

#endif
