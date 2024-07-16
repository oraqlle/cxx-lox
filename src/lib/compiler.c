#include <stdio.h>

#include "compiler.h"
#include "scanner.h"

void compile(VM *vm, Scanner *scanner, const char *source) {
    initScanner(scanner, source);

    size_t line = 0;

    for (;;) {
        Token token = scanToken(scanner);

        if (token.line != line) {
            printf("%4zu ", token.line);
            line = token.line;
        } else {
            printf("   |");
        }

        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF) {
            break;
        }
    }
}
