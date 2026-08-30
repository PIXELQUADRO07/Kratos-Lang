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
        fprintf(stderr, "kratos: out of memory during parsing\n");
        exit(EXIT_FAILURE);
    }
    memcpy(buffer, token.start, token.length);
    buffer[token.length] = '\0';
    return buffer;
}

static char *token_to_number_string(Token token)
{
    char *buffer = token_to_cstring(token);
    char *read = buffer;
    char *write = buffer;
    while (*read != '\0') {
        if (*read != '_') {
            *write++ = *read;
        }
        read++;
    }
    *write = '\0';
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

static void error_at(Parser *parser, Token token, int code, const char *message);

static void advance_token(Parser *parser)
{
    parser->previous = parser->current;
    parser->current = lexer_next_token(parser->lexer);
    if (parser->current.type == TOKEN_ERROR) {
        error_at(parser, parser->current, DIAG_K201, "invalid token");
    }
}

void parser_init(Parser *parser, Lexer *lexer)
{
    parser_init_with_context(parser, lexer, NULL);
}

void parser_init_with_context(Parser *parser, Lexer *lexer, const DiagContext *diag_context)
{
    parser->lexer = lexer;
    parser->had_error = 0;
    parser->diag_context = diag_context;
    parser->current = lexer_next_token(lexer);
    parser->previous = parser->current;
    if (parser->current.type == TOKEN_ERROR) {
        error_at(parser, parser->current, DIAG_K201, "invalid token");
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

static void error_at(Parser *parser, Token token, int code, const char *message)
{
    diag_emitf(parser->diag_context, DIAG_ERROR, code, token.line, token.column,
               token.length, NULL, "%s (found '%.*s')", message,
               (int)token.length, token.start);
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

    error_at(parser, parser->current, DIAG_K202, message);

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
static KratosType parse_type_extended(Parser *parser, char **out_record_name)
{
    if (out_record_name != NULL) {
        *out_record_name = NULL;
    }
    if (check_type_token(parser->current.type)) {
        TokenType type_token = parser->current.type;
        advance_token(parser);
        return token_to_kratos_type(type_token);
    }
    if (check(parser, TOKEN_IDENTIFIER)) {
        Token rec_token = parser->current;
        advance_token(parser);
        if (out_record_name != NULL) {
            *out_record_name = token_to_cstring(rec_token);
        }
        return KRATOS_TYPE_RECORD;
    }

    error_at(parser, parser->current, DIAG_K203,
             "expected a type (k_int, k_float, k_bool, k_char, k_string, k_void or record name)");
    KratosType fallback = KRATOS_TYPE_VOID;
    if (!check(parser, TOKEN_EOF)) {
        advance_token(parser);
    }
    return fallback;
}

static int is_var_decl_starting(Parser *parser)
{
    if (check(parser, TOKEN_K_CONST) || check_type_token(parser->current.type)) {
        return 1;
    }
    if (check(parser, TOKEN_IDENTIFIER)) {
        Parser saved_p = *parser;
        Lexer saved_l = *parser->lexer;

        advance_token(parser);
        while (match(parser, TOKEN_LBRACKET)) {
            if (!match(parser, TOKEN_RBRACKET)) {
                break;
            }
        }
        int is_decl = 0;
        if (check(parser, TOKEN_IDENTIFIER)) {
            advance_token(parser);
            if (check(parser, TOKEN_ASSIGN) || check(parser, TOKEN_CRAFT)) {
                is_decl = 1;
            }
        }

        *parser = saved_p;
        *parser->lexer = saved_l;
        return is_decl;
    }
    return 0;
}


/* --- Espressioni ---
 *
 * Precedenza, dalla piu' bassa alla piu' alta:
 *   or  ->  and  ->  uguaglianza (== !=)  ->  confronto (< > <= >=)
 *   ->  additiva (+ -)  ->  moltiplicativa (* / %)  ->  unaria (not, -)
 *   ->  postfix (indicizzazione, accesso a campi)  ->  primaria
 */

static AstNode *parse_expression(Parser *parser);

static AstNode *parse_primary(Parser *parser)
{
    size_t line = parser->current.line;

    if (match(parser, TOKEN_INTEGER)) {
        char *text = token_to_number_string(parser->previous);
        int64_t value = strtoll(text, NULL, 10);
        free(text);
        return ast_new_literal_int(line, value);
    }

    if (match(parser, TOKEN_FLOAT)) {
        char *text = token_to_number_string(parser->previous);
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
        expect(parser, TOKEN_RBRACKET, "expected ']' after array elements");
        return array_node;
    }

    if (match(parser, TOKEN_LPAREN)) {
        AstNode *inner = parse_expression(parser);
        expect(parser, TOKEN_RPAREN, "expected ')' after expression");
        return inner;
    }

    /* identifier, chiamata "identifier(args)", record literal "Point { ... }" */
    if (check(parser, TOKEN_IDENTIFIER)) {
        Token name_token = parser->current;
        advance_token(parser);
        char *name = token_to_cstring(name_token);

        AstNode *node;

        if (match(parser, TOKEN_LPAREN)) {
            /* invocazione */
            node = ast_new_call_expr(line, name);
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    ast_node_list_push(&node->as.call_expr.arguments, parse_expression(parser));
                } while (match(parser, TOKEN_COMMA));
            }
            expect(parser, TOKEN_RPAREN, "expected ')' after call arguments");
        } else if (match(parser, TOKEN_LBRACE)) {
            /* record literal */
            node = ast_new_record_literal(line, name);
            if (!check(parser, TOKEN_RBRACE)) {
                do {
                    size_t f_line = parser->current.line;
                    Token f_tok = expect(parser, TOKEN_IDENTIFIER, "expected field name in record literal");
                    char *f_name = token_to_cstring(f_tok);
                    expect(parser, TOKEN_COLON, "expected ':' after field name");
                    AstNode *f_val = parse_expression(parser);
                    ast_node_list_push(&node->as.record_literal.fields, ast_new_field_init(f_line, f_name, f_val));
                    free(f_name);
                } while (match(parser, TOKEN_COMMA));
            }
            expect(parser, TOKEN_RBRACE, "expected '}' after record literal fields");
        } else {
            node = ast_new_identifier_expr(line, name);
        }

        free(name);
        return node;
    }

    error_at(parser, parser->current, DIAG_K204, "expected an expression");
    if (!check(parser, TOKEN_EOF)) {
        advance_token(parser);
    }
    /* Nodo segnaposto */
    return ast_new_literal_int(line, 0);
}

static AstNode *parse_postfix(Parser *parser)
{
    AstNode *node = parse_primary(parser);

    while (1) {
        size_t line = parser->current.line;
        if (match(parser, TOKEN_LBRACKET)) {
            AstNode *index = parse_expression(parser);
            expect(parser, TOKEN_RBRACKET, "expected ']' after index");
            node = ast_new_index_expr(line, node, index);
        } else if (match(parser, TOKEN_DOT)) {
            Token member_token = expect(parser, TOKEN_IDENTIFIER, "expected member name after '.'");
            char *member_name = token_to_cstring(member_token);
            node = ast_new_member_expr(line, node, member_name);
            free(member_name);
        } else {
            break;
        }
    }

    return node;
}

/* unary = ( "not" | "-" ) unary | postfix ; */
static AstNode *parse_unary(Parser *parser)
{
    size_t line = parser->current.line;

    if (check(parser, TOKEN_NOT) || check(parser, TOKEN_MINUS)) {
        TokenType op = parser->current.type;
        advance_token(parser);
        AstNode *operand = parse_unary(parser);
        return ast_new_unary_expr(line, op, operand);
    }

    return parse_postfix(parser);
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
 * identificatore o indicizzazione seguito da "=", diventa un nodo AST_ASSIGN;
 * altrimenti l'espressione e' restituita cosi' com'e'.
 */
static AstNode *parse_assignment_or_expression(Parser *parser)
{
    size_t line = parser->current.line;
    AstNode *expr = parse_expression(parser);

    if ((expr->kind == AST_IDENTIFIER_EXPR || expr->kind == AST_INDEX_EXPR || expr->kind == AST_MEMBER_EXPR) &&
        check(parser, TOKEN_ASSIGN)) {
        advance_token(parser);
        AstNode *value = parse_expression(parser);
        return ast_new_assign(line, expr, value);
    }

    return expr;
}


/* --- Statement --- */

static AstNode *parse_statement(Parser *parser);

/* block = "{" { statement } "}" ; */
static AstNode *parse_block(Parser *parser)
{
    size_t line = parser->current.line;
    expect(parser, TOKEN_LBRACE, "expected '{' to start a block");

    AstNode *block = ast_new_block(line);

    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        AstNode *stmt = parse_statement(parser);
        ast_node_list_push(&block->as.block.statements, stmt);
    }

    expect(parser, TOKEN_RBRACE, "expected '}' to close the block");
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
    char *record_name = NULL;
    KratosType type = parse_type_extended(parser, &record_name);

    int is_array = 0;
    while (match(parser, TOKEN_LBRACKET)) {
        expect(parser, TOKEN_RBRACKET, "expected ']' after '[' in array type");
        is_array++;
    }

    Token name_token = expect(parser, TOKEN_IDENTIFIER, "expected a variable name");
    char *name = token_to_cstring(name_token);

    expect(parser, TOKEN_ASSIGN, "expected '=' in declaration (Kratos requires an initializer)");
    AstNode *initializer = parse_expression(parser);
    expect(parser, TOKEN_SEMICOLON, "expected ';' after declaration");

    AstNode *node = ast_new_var_decl(line, is_const, is_array, type, record_name, name, initializer);
    free(record_name);
    free(name);
    return node;
}

/* record_decl = "record" identifier "{" { type [ "[]" ] identifier ";" } "}" ; */
static AstNode *parse_record_decl(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "record" */

    Token name_token = expect(parser, TOKEN_IDENTIFIER, "expected record name after 'record'");
    char *name = token_to_cstring(name_token);

    AstNode *node = ast_new_record_decl(line, name);
    free(name);

    expect(parser, TOKEN_LBRACE, "expected '{' after record name");

    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        size_t field_line = parser->current.line;
        char *field_rec_name = NULL;
        KratosType field_type = parse_type_extended(parser, &field_rec_name);

        int is_array = 0;
        while (match(parser, TOKEN_LBRACKET)) {
            expect(parser, TOKEN_RBRACKET, "expected ']' after '[' in array type");
            is_array++;
        }

        Token field_name_token = expect(parser, TOKEN_IDENTIFIER, "expected field name");
        char *field_name = token_to_cstring(field_name_token);
        expect(parser, TOKEN_SEMICOLON, "expected ';' after field declaration");

        AstNode *field_node = ast_new_record_field(field_line, field_type, is_array, field_rec_name, field_name);
        free(field_rec_name);
        free(field_name);
        ast_node_list_push(&node->as.record_decl.fields, field_node);
    }

    expect(parser, TOKEN_RBRACE, "expected '}' after record declaration");
    return node;
}

/* if_stmt = "if" "(" expression ")" block { "elif" "(" expression ")" block } [ "else" block ] ; */
static AstNode *parse_if_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "if" */

    AstNode *node = ast_new_if_stmt(line);

    expect(parser, TOKEN_LPAREN, "expected '(' after 'if'");
    AstNode *condition = parse_expression(parser);
    expect(parser, TOKEN_RPAREN, "expected ')' after condition");
    AstNode *body = parse_block(parser);
    ast_node_list_push(&node->as.if_stmt.branches, ast_new_cond_branch(line, condition, body));

    while (check(parser, TOKEN_ELIF)) {
        size_t elif_line = parser->current.line;
        advance_token(parser);
        expect(parser, TOKEN_LPAREN, "expected '(' after 'elif'");
        AstNode *elif_condition = parse_expression(parser);
        expect(parser, TOKEN_RPAREN, "expected ')' after condition");
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

    expect(parser, TOKEN_LPAREN, "expected '(' after 'hold'");
    AstNode *condition = parse_expression(parser);
    expect(parser, TOKEN_RPAREN, "expected ')' after condition");
    AstNode *body = parse_block(parser);

    return ast_new_hold_stmt(line, condition, body);
}

/* press_stmt = "press" block "hold" "(" expression ")" ";" ; */
static AstNode *parse_press_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "press" */

    AstNode *body = parse_block(parser);

    expect(parser, TOKEN_HOLD, "expected 'hold (condition);' after 'press' body");
    expect(parser, TOKEN_LPAREN, "expected '(' after 'hold'");
    AstNode *condition = parse_expression(parser);
    expect(parser, TOKEN_RPAREN, "expected ')' after condition");
    expect(parser, TOKEN_SEMICOLON, "expected ';' after 'press' condition");

    return ast_new_press_stmt(line, body, condition);
}

/* drive_stmt = "drive" "(" declaration expression ";" expression ")" block ; */
static AstNode *parse_drive_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "drive" */

    expect(parser, TOKEN_LPAREN, "expected '(' after 'drive'");
    AstNode *init = parse_var_decl(parser); /* consuma gia' il ';' dell'inizializzatore */
    AstNode *condition = parse_expression(parser);
    expect(parser, TOKEN_SEMICOLON, "expected ';' after 'drive' condition");
    AstNode *step = parse_assignment_or_expression(parser);
    expect(parser, TOKEN_RPAREN, "expected ')' after 'drive' step");
    AstNode *body = parse_block(parser);

    return ast_new_drive_stmt(line, init, condition, step, body);
}

/* sweep_stmt = "sweep" "(" type identifier "in" identifier ")" block ; */
static AstNode *parse_sweep_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "sweep" */

    expect(parser, TOKEN_LPAREN, "expected '(' after 'sweep'");
    char *elem_rec_name = NULL;
    KratosType element_type = parse_type_extended(parser, &elem_rec_name);
    Token elem_token = expect(parser, TOKEN_IDENTIFIER, "expected the element name in 'sweep'");
    char *element_name = token_to_cstring(elem_token);

    expect(parser, TOKEN_IN, "expected 'in' in 'sweep (type name in collection)'");

    Token coll_token = expect(parser, TOKEN_IDENTIFIER, "expected the collection name in 'sweep'");
    char *collection_name = token_to_cstring(coll_token);

    expect(parser, TOKEN_RPAREN, "expected ')' after 'sweep' collection");
    AstNode *body = parse_block(parser);

    AstNode *node = ast_new_sweep_stmt(line, element_type, elem_rec_name, element_name, collection_name, body);
    free(elem_rec_name);
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
    expect(parser, TOKEN_SEMICOLON, "expected ';' after 'yield'");

    return ast_new_yield_stmt(line, value);
}

/* "shout" "(" expression ")" ";" */
static AstNode *parse_shout_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "shout" */

    expect(parser, TOKEN_LPAREN, "expected '(' after 'shout'");
    AstNode *value = parse_expression(parser);
    expect(parser, TOKEN_RPAREN, "expected ')' after 'shout' expression");
    expect(parser, TOKEN_SEMICOLON, "expected ';' after 'shout(...)'");

    return ast_new_shout_stmt(line, value);
}

/* "wield" string ";" */
static AstNode *parse_wield_stmt(Parser *parser)
{
    size_t line = parser->current.line;
    advance_token(parser); /* consuma "wield" */

    Token path_token = expect(parser, TOKEN_STRING_LITERAL, "expected a quoted path after 'wield'");
    char *path = token_to_string_value(path_token);
    expect(parser, TOKEN_SEMICOLON, "expected ';' after 'wield \"path\"'");

    AstNode *node = ast_new_wield_stmt(line, path);
    free(path);
    return node;
}

/* expr_stmt = expression ";" ; oppure assignment = identifier "=" expression ";" ; */
static AstNode *parse_expression_statement(Parser *parser)
{
    size_t line = parser->current.line;
    AstNode *result = parse_assignment_or_expression(parser);
    expect(parser, TOKEN_SEMICOLON, "expected ';' after statement");

    if (result->kind == AST_ASSIGN) {
        return result;
    }
    return ast_new_expr_stmt(line, result);
}

static AstNode *parse_statement(Parser *parser)
{
    if (check(parser, TOKEN_RECORD)) {
        return parse_record_decl(parser);
    }

    if (is_var_decl_starting(parser)) {
        return parse_var_decl(parser);
    }

    switch (parser->current.type) {
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
            expect(parser, TOKEN_SEMICOLON, "expected ';' after 'snap'");
            return ast_new_snap_stmt(line);
        }

        case TOKEN_PUSH: {
            size_t line = parser->current.line;
            advance_token(parser);
            expect(parser, TOKEN_SEMICOLON, "expected ';' after 'push'");
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
static AstNode *parse_function_decl(Parser *parser, KratosType return_type, const char *return_record_name, size_t line)
{
    advance_token(parser); /* consuma "craft" */

    Token name_token = expect(parser, TOKEN_IDENTIFIER, "expected a function name after 'craft'");
    char *name = token_to_cstring(name_token);

    AstNode *func = ast_new_func_decl(line, return_type, return_record_name, name, NULL);
    free(name);

    expect(parser, TOKEN_LPAREN, "expected '(' after function name");

    if (!check(parser, TOKEN_RPAREN)) {
        do {
            size_t param_line = parser->current.line;
            char *param_rec_name = NULL;
            KratosType param_type = parse_type_extended(parser, &param_rec_name);
            int is_array = 0;
            while (match(parser, TOKEN_LBRACKET)) {
                expect(parser, TOKEN_RBRACKET, "expected ']' after '[' in array type");
                is_array++;
            }
            Token param_name_token = expect(parser, TOKEN_IDENTIFIER, "expected a parameter name");
            char *param_name = token_to_cstring(param_name_token);
            ast_node_list_push(&func->as.func_decl.params, ast_new_param(param_line, param_type, is_array, param_rec_name, param_name));
            free(param_rec_name);
            free(param_name);
        } while (match(parser, TOKEN_COMMA));
    }

    expect(parser, TOKEN_RPAREN, "expected ')' after parameters");

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

    if (check(parser, TOKEN_RECORD)) {
        return parse_record_decl(parser);
    }

    if (check(parser, TOKEN_WIELD)) {
        return parse_wield_stmt(parser);
    }

    int is_const = match(parser, TOKEN_K_CONST);
    char *record_name = NULL;
    KratosType type = parse_type_extended(parser, &record_name);

    if (!is_const && check(parser, TOKEN_CRAFT)) {
        AstNode *func = parse_function_decl(parser, type, record_name, line);
        free(record_name);
        return func;
    }

    int is_array = 0;
    while (match(parser, TOKEN_LBRACKET)) {
        expect(parser, TOKEN_RBRACKET, "expected ']' after '[' in array type");
        is_array++;
    }

    Token name_token = expect(parser, TOKEN_IDENTIFIER, "expected a variable name or 'craft' function");
    char *name = token_to_cstring(name_token);

    expect(parser, TOKEN_ASSIGN, "expected '=' in declaration (Kratos requires an initializer)");
    AstNode *initializer = parse_expression(parser);
    expect(parser, TOKEN_SEMICOLON, "expected ';' after declaration");

    AstNode *node = ast_new_var_decl(line, is_const, is_array, type, record_name, name, initializer);
    free(record_name);
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
