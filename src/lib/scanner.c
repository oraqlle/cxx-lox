#include "scanner.h"

#include <string.h>

void initScanner(Scanner *scanner, const char *source) {
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

static bool isAtEnd(Scanner *scanner) { return *scanner->current == '\0'; }

static char advance(Scanner *scanner) {
    scanner->current++;
    return scanner->current[-1];
}

static char peek(Scanner *scanner) { return *scanner->current; }

static char peekNext(Scanner *scanner) {
    if (isAtEnd(scanner)) {
        return '\0';
    }

    return scanner->current[1];
}

static bool match(Scanner *scanner, char expected) {
    if (isAtEnd(scanner)) {
        return false;
    }

    if (*scanner->current != expected) {
        return false;
    }

    scanner->current++;
    return true;
}

static void skipWhitespace(Scanner *scanner) {
    for (;;) {
        char chr = peek(scanner);

        switch (chr) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                if (peekNext(scanner) == '/') {
                    while (peek(scanner) != '\n' && !isAtEnd(scanner)) {
                        advance(scanner);
                    }
                } else {
                    return;
                }
                break;
            default:
                return; // Unreachable
        }
    }
}

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

static Token string(Scanner *scanner) {
    while (peek(scanner) != '"' && !isAtEnd(scanner)) {
        if (peek(scanner) == '\n') {
            scanner->line++;
        }

        advance(scanner);
    }

    if (isAtEnd(scanner)) {
        return errorToken(scanner, "Unterminated string literal.");
    }

    // Closing quote
    advance(scanner);
    return makeToken(scanner, TOKEN_STRING);
}

static bool isDigit(char chr) { return chr >= '0' && chr <= '9'; }

static Token number(Scanner *scanner) {
    while (isDigit(peek(scanner))) {
        advance(scanner);
    }

    // Look for fractional part
    if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        // Consume decimal point '.'
        advance(scanner);

        while (isDigit(peek(scanner))) {
            advance(scanner);
        }
    }

    return makeToken(scanner, TOKEN_NUMBER);
}

static bool isAlpha(char chr) {
    return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || chr == '_';
}

static TokenType checkKeyword(Scanner *scanner, ptrdiff_t start, ptrdiff_t length,
                              const char *rest, TokenType type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp((const void *)(scanner->start + start), (const void *)rest,
               (size_t)length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner *scanner) {
    switch (scanner->start[0]) {
        case 'a':
            return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND);
        case 'c':
            return checkKeyword(scanner, 1, 4, "lass", TOKEN_CLASS);
        case 'e':
            return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a':
                        return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
        case 'n':
            return checkKeyword(scanner, 1, 2, "il", TOKEN_NIL);
        case 'o':
            return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p':
            return checkKeyword(scanner, 1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return checkKeyword(scanner, 1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h':
                        return checkKeyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return checkKeyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v':
            return checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w':
            return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *scanner) {
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) {
        advance(scanner);
    }

    return makeToken(scanner, identifierType(scanner));
}

Token scanToken(Scanner *scanner) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) {
        return makeToken(scanner, TOKEN_EOF);
    }

    char chr = advance(scanner);

    if (isAlpha(chr)) {
        return identifier(scanner);
    }

    if (isDigit(chr)) {
        return number(scanner);
    }

    switch (chr) {
        case '(':
            return makeToken(scanner, TOKEN_LEFT_PAREN);
        case ')':
            return makeToken(scanner, TOKEN_RIGHT_PAREN);
        case '{':
            return makeToken(scanner, TOKEN_LEFT_BRACE);
        case '}':
            return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case ';':
            return makeToken(scanner, TOKEN_SEMICOLON);
        case ',':
            return makeToken(scanner, TOKEN_COMMA);
        case '.':
            return makeToken(scanner, TOKEN_DOT);
        case '-':
            return makeToken(scanner, TOKEN_MINUS);
        case '+':
            return makeToken(scanner, TOKEN_PLUS);
        case '/':
            return makeToken(scanner, TOKEN_SLASH);
        case '*':
            return makeToken(scanner, TOKEN_STAR);
        case '!':
            return makeToken(scanner,
                             match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(scanner,
                             match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(scanner,
                             match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(scanner,
                             match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return string(scanner);
    }

    return errorToken(scanner, "Unexpected character.");
}
