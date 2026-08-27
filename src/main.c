#include <stdio.h>

#include "lexer/lexer.h"

int main(void)
{
    const char *source =
    "k_int Numero = 10;\n"
    "k_float Pi = 3.14;\n"
    "k_bool Verifica = true;\n"
    "k_bool AltraVerifica = false;\n"
    "k_char Lettera = 'K';\n"
    "k_string Nome = \"Kratos\";\n"
    "craft Test\n";

    Lexer lexer;

    lexer_init(&lexer, source);

    Token token;

    do{
        token = lexer_next_token(&lexer);

        printf(
            "line %zu | %-15s %.*s\n",
            token.line,
            token_type_name(token.type),
            (int)token.length,
            token.start
        );
    } while (token.type != TOKEN_EOF);

    return 0;
}