#include "scanner.h"
#include <string.h>

void initScanner(Scanner *scanner, const char *source) {
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

static bool isAtEnd(Scanner *scanner) { return *scanner->current == '\0'; }

static Token makeToken(Scanner *scanner, TokenType type) {
    Token token;

    token.type = type;
    token.start = scanner->start;
    token.length = (size_t)(scanner->current - scanner->start);
    token.line = scanner->line;

    return token;
}

static Token errorToken(Scanner *scanner, const char *message) {
    Token token;

    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (size_t)strlen(message);
    token.line = scanner->line;

    return token;
}

Token scanToken(Scanner *scanner) {
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) {
        return makeToken(scanner, TOKEN_EOF);
    }

    return errorToken(scanner, "Unexpected character.");
}
