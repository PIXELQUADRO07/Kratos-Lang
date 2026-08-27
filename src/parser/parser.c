#include "parser/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Helper sui token --- */

/*
 * Copia un token in una stringa C null-terminated appena allocata.
 * Necessario perche' Token.start punta dentro al buffer sorgente originale
 * e Token.length ne delimita l'estensione: i costruttori dell'AST si
 * aspettano invece stringhe C "normali" e ne fanno una loro copia interna,
 * quindi il buffer temporaneo restituito qui va liberato dal chiamante
 * subito dopo l'uso.
 */
static char *token_to_cstring(Token token)
{
    char *buffer = malloc(token.length + 1);
    if (buffer == NULL) {
        fprintf(stderr, "kratos: memoria esaurita durante il parsing\n");
        exit(EXIT_FAILURE);
    }
    memcpy(buffer, token.start, token.length);
    buffer[token.length] = '\0';
    return buffer;
}

static char *token_to_string_value(Token token)
{
    return token_decode_string(token);
}

static char token_to_char_value(Token token)
{
    char value = '\0';
    if (!token_decode_char(token, &value)) {
        return '\0';
    }
    return value;
}


/* --- Stato del parser --- */

static void error_at(Parser *parser, Token token, const char *message);

static void advance_token(Parser *parser)
{
    parser->previous = parser->current;
    parser->current = lexer_next_token(parser->lexer);
    if (parser->current.type == TOKEN_ERROR) {
        error_at(parser, parser->current, "token non valido");
    }
}

void parser_init(Parser *parser, Lexer *lexer)
{
    parser->lexer = lexer;
    parser->had_error = 0;
    parser->current = lexer_next_token(lexer);
    parser->previous = parser->current;
    if (parser->current.type == TOKEN_ERROR) {
        error_at(parser, parser->current, "token non valido");
    }
}

static int check(Parser *parser, TokenType type)
{
    return parser->current.type == type;
}

/*
 * Se il token corrente e' del tipo dato, lo consuma e restituisce 1.
 * Altrimenti non consuma nulla e restituisce 0.
 */
static int match(Parser *parser, TokenType type)
{
    if (!check(parser, type)) {
        return 0;
    }
    advance_token(parser);
    return 1;
}

static void error_at(Parser *parser, Token token, const char *message)
{
    fprintf(
        stderr,
        "kratos: errore di sintassi alla riga %zu: %s (trovato '%.*s')\n",
        token.line,
        message,
        (int)token.length,
        token.start
    );
    parser->had_error = 1;
}

/*
 * Consuma il token corrente se e' del tipo atteso e lo restituisce.
 * Altrimenti segnala un errore e consuma comunque il token corrente, per
 * evitare che il parser resti bloccato in un ciclo infinito: il resto del
 * file viene comunque analizzato, cosi' da poter riportare piu' di un
 * errore per volta. L'AST prodotto in presenza di errori non va considerato
 * valido (controllare parser->had_error).
 */
static Token expect(Parser *parser, TokenType type, const char *message)
{
    if (check(parser, type)) {
        Token token = parser->current;
        advance_token(parser);
        return token;
    }

    error_at(parser, parser->current, message);

    Token bad_token = parser->current;
    if (!check(parser, TOKEN_EOF)) {
        advance_token(parser);
    }
    return bad_token;
}

static int check_type_token(TokenType type)
{
    switch (type) {
        case TOKEN_K_INT:
        case TOKEN_K_FLOAT:
        case TOKEN_K_BOOL:
        case TOKEN_K_CHAR:
        case TOKEN_K_STRING:
        case TOKEN_K_VOID:
            return 1;
        default:
            return 0;
    }
}

static KratosType token_to_kratos_type(TokenType type)
{
    switch (type) {
        case TOKEN_K_INT:    return KRATOS_TYPE_INT;
        case TOKEN_K_FLOAT:  return KRATOS_TYPE_FLOAT;
        case TOKEN_K_BOOL:   return KRATOS_TYPE_BOOL;
        case TOKEN_K_CHAR:   return KRATOS_TYPE_CHAR;
        case TOKEN_K_STRING: return KRATOS_TYPE_STRING;
        case TOKEN_K_VOID:   return KRATOS_TYPE_VOID;
        default:             return KRATOS_TYPE_VOID; /* non dovrebbe accadere */
    }
}

/*
 * type = "k_int" | "k_float" | "k_bool" | "k_char" | "k_string" | "k_void" ;
 *
 * k_void e' sintatticamente ammesso qui anche per una variabile: il divieto
 * ("k_void e' solo un tipo di ritorno") e' una regola semantica, non
 * grammaticale, e verra' applicata quando esistera' un semantic analyzer
 * (vedi docs/types.md).
 */
static KratosType parse_type(Parser *parser)
{
    if (!check_type_token(parser->current.type)) {
        error_at(parser, parser->current, "atteso un tipo (k_int, k_float, k_bool, k_char, k_string o k_void)");
        KratosType fallback = KRATOS_TYPE_VOID;
        if (!check(parser, TOKEN_EOF)) {
            advance_token(parser);
        }
        return fallback;
    }

    TokenType type_token = parser->current.type;
    advance_token(parser);
    return token_to_kratos_type(type_token);
}


/* --- Espressioni ---
 *
 * Precedenza, dalla piu' bassa alla piu' alta:
 *   or  ->  and  ->  uguaglianza (== !=)  ->  confronto (< > <= >=)
 *   ->  additiva (+ -)  ->  moltiplicativa (* / %)  ->  unaria (not, -)
 *   ->  postfix (indicizzazione)  ->  primaria
 */

static AstNode *parse_expression(Parser *parser);

static AstNode *parse_primary(Parser *parser)
{
    size_t line = parser->current.line;

    if (match(parser, TOKEN_INTEGER)) {
        char *text = token_to_cstring(parser->previous);
        int64_t value = strtoll(text, NULL, 10);
        free(text);
        return ast_new_literal_int(line, value);
    }

    if (match(parser, TOKEN_FLOAT)) {
        char *text = token_to_cstring(parser->previous);
        double value = strtod(text, NULL);
        free(text);
        return ast_new_literal_float(line, value);
    }

    if (match(parser, TOKEN_TRUE)) {
        return ast_new_literal_bool(line, 1);
    }

    if (match(parser, TOKEN_FALSE)) {
        return ast_new_literal_bool(line, 0);
    }

    if (match(parser, TOKEN_CHAR_LITERAL)) {
        return ast_new_literal_char(line, token_to_char_value(parser->previous));
    }

    if (match(parser, TOKEN_STRING_LITERAL)) {
        char *value = token_to_string_value(parser->previous);
        AstNode *node = ast_new_literal_string(line, value);
        free(value);
        return node;
    }

    /* array_literal = "[" [ expression { "," expression } ] "]" ; */
    if (match(parser, TOKEN_LBRACKET)) {
        AstNode *array_node = ast_new_array_literal(line);
        if (!check(parser, TOKEN_RBRACKET)) {
            do {
                ast_node_list_push(&array_node->as.array_literal.elements, parse_expression(parser));
            } while (match(parser, TOKEN_COMMA));
        }
        expect(parser, TOKEN_RBRACKET, "atteso ']' dopo gli elementi dell'array");
        return array_node;
    }

    if (match(parser, TOKEN_LPAREN)) {
        AstNode *inner = parse_expression(parser);
        expect(parser, TOKEN_RPAREN, "atteso ')' dopo l'espressione");
        return inner;
    }

    /* identifier, chiamata "identifier(args)" o indicizzazione "identifier[expr]" */
    if (check(parser, TOKEN_IDENTIFIER)) {
        Token name_token = parser->current;
        advance_token(parser);
        char *name = token_to_cstring(name_token);

        AstNode *node;

        if (match(parser, TOKEN_LPAREN)) {
            /* invocazione: sintassi decisa in docs/functions.md, non ancora nella grammatica formale */
            node = ast_new_call_expr(line, name);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    ast_node_list_push(&node->as.call_expr.arguments, parse_expression(parser));
                } while (match(parser, TOKEN_COMMA));
            }
            expect(parser, TOKEN_RPAREN, "atteso ')' dopo gli argomenti della chiamata");
        } else {
            node = ast_new_identifier_expr(line, name);
        }

        free(name);

        /* indicizzazione postfix: identifier[expr] o name(args)[expr] */
        while (match(parser, TOKEN_LBRACKET)) {
            AstNode *index = parse_expression(parser);
            expect(parser, TOKEN_RBRACKET, "atteso ']' dopo l'indice");
            node = ast_new_index_expr(line, node, index);
        }

        return node;
    }

    error_at(parser, parser->current, "espressione attesa");
    if (!check(parser, TOKEN_EOF)) {
        advance_token(parser);
    }
    /* Nodo segnaposto, cosi' il chiamante puo' proseguire senza controllare NULL ovunque. */
    return ast_new_literal_int(line, 0);
}

/* unary = ( "not" | "-" ) unary | postfix_primary ; */
static AstNode *parse_unary(Parser *parser)
{
    size_t line = parser->current.line;

    if (check(parser, TOKEN_NOT) || check(parser, TOKEN_MINUS)) {
        TokenType op = parser->current.type;
        advance_token(parser);
        AstNode *operand = parse_unary(parser);
        return ast_new_unary_expr(line, op, operand);
    }

    return parse_primary(parser);
}

static AstNode *parse_multiplicative(Parser *parser)
{
    AstNode *left = parse_unary(parser);

    while (check(parser, TOKEN_STAR) || check(parser, TOKEN_SLASH) || check(parser, TOKEN_PERCENT)) {
        size_t line = parser->current.line;
        TokenType op = parser->current.type;
        advance_token(parser);
        AstNode *right = parse_unary(parser);
        left = ast_new_binary_expr(line, op, left, right);
    }

    return left;
}

static AstNode *parse_additive(Parser *parser)
{
    AstNode *left = parse_multiplicative(parser);

    while (check(parser, TOKEN_PLUS) || check(parser, TOKEN_MINUS)) {
        size_t line = parser->current.line;
        TokenType op = parser->current.type;
        advance_token(parser);
        AstNode *right = parse_multiplicative(parser);
        left = ast_new_binary_expr(line, op, left, right);
    }

    return left;
}

static AstNode *parse_comparison(Parser *parser)
{
    AstNode *left = parse_additive(parser);

    while (check(parser, TOKEN_LESS) || check(parser, TOKEN_GREATER) ||
           check(parser, TOKEN_LESS_EQUAL) || check(parser, TOKEN_GREATER_EQUAL)) {
        size_t line = parser->current.line;
        TokenType op = parser->current.type;
        advance_token(parser);
        AstNode *right = parse_additive(parser);
        left = ast_new_binary_expr(line, op, left, right);
    }

    return left;
}

static AstNode *parse_equality(Parser *parser)
{
    AstNode *left = parse_comparison(parser);

    while (check(parser, TOKEN_EQUAL) || check(parser, TOKEN_NOT_EQUAL)) {
        size_t line = parser->current.line;
        TokenType op = parser->current.type;
        advance_token(parser);
        AstNode *right = parse_comparison(parser);
        left = ast_new_binary_expr(line, op, left, right);
    }

    return left;
}

static AstNode *parse_and(Parser *parser)
{
    AstNode *left = parse_equality(parser);

    while (check(parser, TOKEN_AND)) {
        size_t line = parser->current.line;
        advance_token(parser);
        AstNode *right = parse_equality(parser);
        left = ast_new_binary_expr(line, TOKEN_AND, left, right);
    }

    return left;
}

static AstNode *parse_or(Parser *parser)
{
    AstNode *left = parse_and(parser);

    while (check(parser, TOKEN_OR)) {
        size_t line = parser->current.line;
        advance_token(parser);
        AstNode *right = parse_and(parser);
        left = ast_new_binary_expr(line, TOKEN_OR, left, right);
    }

    return left;
}

static AstNode *parse_expression(Parser *parser)
{
    return parse_or(parser);
}

/*
 * Espressione oppure assegnamento, SENZA consumare il ';' finale: usata sia
 * per lo statement di assegnamento sia per la clausola di step di "drive"
 * (che non ha ';' prima della ")"). Se l'espressione analizzata e' un
 * semplice identificatore seguito da "=", diventa un nodo AST_ASSIGN;
 * altrimenti l'espressione e' restituita cosi' com'e'.
 */
static AstNode *parse_assignment_or_expression(Parser *parser)
{
    size_t line = parser->current.line;
    AstNode *expr = parse_expression(parser);

    if (expr->kind == AST_IDENTIFIER_EXPR && check(parser, TOKEN_ASSIGN)) {
        advance_token(parser);
        AstNode *value = parse_expression(parser);
        AstNode *assign = ast_new_assign(line, expr->as.identifier_expr.name, value);
        ast_free(expr);
        return assign;
    }

    return expr;
}


/* --- Statement --- */

static AstNode *parse_statement(Parser *parser);

/* block = "{" { statement } "}" ; */
static AstNode *parse_block(Parser *parser)
{
    size_t line = parser->current.line;
    expect(parser, TOKEN_LBRACE, "atteso '{' per iniziare un blocco");

    AstNode *block = ast_new_block(line);

    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        AstNode *stmt = parse_statement(parser);
        ast_node_list_push(&block->as.block.statements, stmt);
    }

    expect(parser, TOKEN_RBRACE, "atteso '}' per chiudere il blocco");
    return block;
}

/*
 * declaration = [ "k_const" ] type [ "[" "]" ] identifier "=" expression ";" ;
 * Usata sia a livello di statement (variabili locali) sia come init di "drive".
 */
static AstNode *parse_var_decl(Parser *parser)
{
    size_t line = parser->current.line;

    int is_const = match(parser, TOKEN_K_CONST);
    KratosType type = parse_type(parser);

    int is_array = 0;
    if (match(parser, TOKEN_LBRACKET)) {
        expect(parser, TOKEN_RBRACKET, "atteso ']' dopo '[' nel tipo array");
        is_array = 1;
    }

    Token name_token = expect(parser, TOKEN_IDENTIFIER, "atteso il nome della variabile");
    char *name = token_to_cstring(name_token);

    expect(parser, TOKEN_ASSIGN, "atteso '=' nella dichiarazione (Kratos richiede sempre un valore iniziale)");
    AstNode *initializer = parse_expression(parser);
    expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo la dichiarazione");

    AstNode *node = ast_new_var_decl(line, is_const, is_array, type, name, initializer);
    free(name);
    return node;
}

/* if_stmt = "if" "(" expression ")" block { "elif" "(" expression ")" block } [ "else" block ] ; */
static AstNode *parse_if_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "if" */

    AstNode *node = ast_new_if_stmt(line);

    expect(parser, TOKEN_LPAREN, "atteso '(' dopo 'if'");
    AstNode *condition = parse_expression(parser);
    expect(parser, TOKEN_RPAREN, "atteso ')' dopo la condizione");
    AstNode *body = parse_block(parser);
    ast_node_list_push(&node->as.if_stmt.branches, ast_new_cond_branch(line, condition, body));

    while (check(parser, TOKEN_ELIF)) {
        size_t elif_line = parser->current.line;
        advance_token(parser);
        expect(parser, TOKEN_LPAREN, "atteso '(' dopo 'elif'");
        AstNode *elif_condition = parse_expression(parser);
        expect(parser, TOKEN_RPAREN, "atteso ')' dopo la condizione");
        AstNode *elif_body = parse_block(parser);
        ast_node_list_push(&node->as.if_stmt.branches, ast_new_cond_branch(elif_line, elif_condition, elif_body));
    }

    if (match(parser, TOKEN_ELSE)) {
        node->as.if_stmt.else_block = parse_block(parser);
    }

    return node;
}

/* hold_stmt = "hold" "(" expression ")" block ; */
static AstNode *parse_hold_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "hold" */

    expect(parser, TOKEN_LPAREN, "atteso '(' dopo 'hold'");
    AstNode *condition = parse_expression(parser);
    expect(parser, TOKEN_RPAREN, "atteso ')' dopo la condizione");
    AstNode *body = parse_block(parser);

    return ast_new_hold_stmt(line, condition, body);
}

/* press_stmt = "press" block "hold" "(" expression ")" ";" ; */
static AstNode *parse_press_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "press" */

    AstNode *body = parse_block(parser);

    expect(parser, TOKEN_HOLD, "atteso 'hold (condizione);' dopo il corpo di 'press'");
    expect(parser, TOKEN_LPAREN, "atteso '(' dopo 'hold'");
    AstNode *condition = parse_expression(parser);
    expect(parser, TOKEN_RPAREN, "atteso ')' dopo la condizione");
    expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo 'hold (condizione)' di 'press'");

    return ast_new_press_stmt(line, body, condition);
}

/* drive_stmt = "drive" "(" declaration expression ";" expression ")" block ; */
static AstNode *parse_drive_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "drive" */

    expect(parser, TOKEN_LPAREN, "atteso '(' dopo 'drive'");
    AstNode *init = parse_var_decl(parser); /* consuma gia' il ';' dell'inizializzatore */
    AstNode *condition = parse_expression(parser);
    expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo la condizione di 'drive'");
    AstNode *step = parse_assignment_or_expression(parser);
    expect(parser, TOKEN_RPAREN, "atteso ')' dopo lo step di 'drive'");
    AstNode *body = parse_block(parser);

    return ast_new_drive_stmt(line, init, condition, step, body);
}

/* sweep_stmt = "sweep" "(" type identifier "in" identifier ")" block ; */
static AstNode *parse_sweep_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "sweep" */

    expect(parser, TOKEN_LPAREN, "atteso '(' dopo 'sweep'");
    KratosType element_type = parse_type(parser);
    Token elem_token = expect(parser, TOKEN_IDENTIFIER, "atteso il nome dell'elemento in 'sweep'");
    char *element_name = token_to_cstring(elem_token);

    expect(parser, TOKEN_IN, "atteso 'in' in 'sweep (tipo nome in collezione)'");

    Token coll_token = expect(parser, TOKEN_IDENTIFIER, "atteso il nome della collezione in 'sweep'");
    char *collection_name = token_to_cstring(coll_token);

    expect(parser, TOKEN_RPAREN, "atteso ')' dopo la collezione di 'sweep'");
    AstNode *body = parse_block(parser);

    AstNode *node = ast_new_sweep_stmt(line, element_type, element_name, collection_name, body);
    free(element_name);
    free(collection_name);
    return node;
}

/* "yield" [ expression ] ";" */
static AstNode *parse_yield_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "yield" */

    AstNode *value = NULL;
    if (!check(parser, TOKEN_SEMICOLON)) {
        value = parse_expression(parser);
    }
    expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo 'yield'");

    return ast_new_yield_stmt(line, value);
}

/* "shout" "(" expression ")" ";" */
static AstNode *parse_shout_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "shout" */

    expect(parser, TOKEN_LPAREN, "atteso '(' dopo 'shout'");
    AstNode *value = parse_expression(parser);
    expect(parser, TOKEN_RPAREN, "atteso ')' dopo l'espressione di 'shout'");
    expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo 'shout(...)'");

    return ast_new_shout_stmt(line, value);
}

/* "wield" string ";" */
static AstNode *parse_wield_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "wield" */

    Token path_token = expect(parser, TOKEN_STRING_LITERAL, "atteso il percorso tra virgolette dopo 'wield'");
    char *path = token_to_string_value(path_token);
    expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo 'wield \"percorso\"'");

    AstNode *node = ast_new_wield_stmt(line, path);
    free(path);
    return node;
}

/* expr_stmt = expression ";" ; oppure assignment = identifier "=" expression ";" ; */
static AstNode *parse_expression_statement(Parser *parser)
{
    size_t line = parser->current.line;
    AstNode *result = parse_assignment_or_expression(parser);
    expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo l'istruzione");

    if (result->kind == AST_ASSIGN) {
        return result;
    }
    return ast_new_expr_stmt(line, result);
}

static AstNode *parse_statement(Parser *parser)
{
    switch (parser->current.type) {
        case TOKEN_K_INT:
        case TOKEN_K_FLOAT:
        case TOKEN_K_BOOL:
        case TOKEN_K_CHAR:
        case TOKEN_K_STRING:
        case TOKEN_K_VOID:
        case TOKEN_K_CONST:
            return parse_var_decl(parser);

        case TOKEN_IF:
            return parse_if_stmt(parser);

        case TOKEN_HOLD:
            return parse_hold_stmt(parser);

        case TOKEN_PRESS:
            return parse_press_stmt(parser);

        case TOKEN_DRIVE:
            return parse_drive_stmt(parser);

        case TOKEN_SWEEP:
            return parse_sweep_stmt(parser);

        case TOKEN_SNAP: {
            size_t line = parser->current.line;
            advance_token(parser);
            expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo 'snap'");
            return ast_new_snap_stmt(line);
        }

        case TOKEN_PUSH: {
            size_t line = parser->current.line;
            advance_token(parser);
            expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo 'push'");
            return ast_new_push_stmt(line);
        }

        case TOKEN_YIELD:
            return parse_yield_stmt(parser);

        case TOKEN_SHOUT:
            return parse_shout_stmt(parser);

        case TOKEN_WIELD:
            return parse_wield_stmt(parser);

        case TOKEN_LBRACE:
            /* Blocco annidato "nudo": non nella grammatica formale, ma utile e innocuo. */
            return parse_block(parser);

        default:
            return parse_expression_statement(parser);
    }
}


/* --- Dichiarazioni di primo livello --- */

/* function = return_type "craft" identifier "(" [ params ] ")" block ; */
static AstNode *parse_function_decl(Parser *parser, KratosType return_type, size_t line)
{
    advance_token(parser); /* consuma "craft" */

    Token name_token = expect(parser, TOKEN_IDENTIFIER, "atteso il nome della funzione dopo 'craft'");
    char *name = token_to_cstring(name_token);

    AstNode *func = ast_new_func_decl(line, return_type, name, NULL);
    free(name);

    expect(parser, TOKEN_LPAREN, "atteso '(' dopo il nome della funzione");

    if (!check(parser, TOKEN_RPAREN)) {
        do {
            size_t param_line = parser->current.line;
            KratosType param_type = parse_type(parser);
            Token param_name_token = expect(parser, TOKEN_IDENTIFIER, "atteso il nome del parametro");
            char *param_name = token_to_cstring(param_name_token);
            ast_node_list_push(&func->as.func_decl.params, ast_new_param(param_line, param_type, param_name));
            free(param_name);
        } while (match(parser, TOKEN_COMMA));
    }

    expect(parser, TOKEN_RPAREN, "atteso ')' dopo i parametri");

    func->as.func_decl.body = parse_block(parser);

    return func;
}

/*
 * program = { declaration | function } ;
 *
 * Le due produzioni condividono il prefisso "[ k_const ] type": la scelta
 * tra le due si fa guardando se dopo il tipo arriva "craft".
 */
static AstNode *parse_top_level(Parser *parser)
{
    size_t line = parser->current.line;

    if (check(parser, TOKEN_WIELD)) {
        return parse_wield_stmt(parser);
    }

    int is_const = match(parser, TOKEN_K_CONST);
    KratosType type = parse_type(parser);

    if (!is_const && check(parser, TOKEN_CRAFT)) {
        return parse_function_decl(parser, type, line);
    }

    int is_array = 0;
    if (match(parser, TOKEN_LBRACKET)) {
        expect(parser, TOKEN_RBRACKET, "atteso ']' dopo '[' nel tipo array");
        is_array = 1;
    }

    Token name_token = expect(parser, TOKEN_IDENTIFIER, "atteso il nome della variabile o 'craft' per una funzione");
    char *name = token_to_cstring(name_token);

    expect(parser, TOKEN_ASSIGN, "atteso '=' nella dichiarazione (Kratos richiede sempre un valore iniziale)");
    AstNode *initializer = parse_expression(parser);
    expect(parser, TOKEN_SEMICOLON, "atteso ';' dopo la dichiarazione");

    AstNode *node = ast_new_var_decl(line, is_const, is_array, type, name, initializer);
    free(name);
    return node;
}

AstNode *parser_parse_program(Parser *parser)
{
    size_t line = parser->current.line;
    AstNode *program = ast_new_program(line);

    while (!check(parser, TOKEN_EOF)) {
        AstNode *decl = parse_top_level(parser);
        ast_node_list_push(&program->as.program.declarations, decl);
    }

    return program;
}
