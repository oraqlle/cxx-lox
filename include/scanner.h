/**
 * @brief Tokenizes Lox source
 *
 * @file scanner.h
 */

#ifndef clox_scanner_h
#define clox_scanner_h

#include "common.h"

/**
 * @brief Scanner object used to store state of current scan pass.
 */
typedef struct {
    const char *start;
    const char *current;
    size_t line;
} Scanner;

/**
 * @brief Token type enum.
 */
typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,

    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,

    // One or two character tokens.
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,

    // Literals
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,

    // Keywords.
    TOKEN_AND,
    TOKEN_CLASS,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_NIL,
    TOKEN_OR,
    TOKEN_PRINT,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,

    // Misc..
    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

/**
 * @brief Token type.
 */
typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    size_t line;
} Token;

/**
 * @brief Initializes Scanner.
 */
void initScanner(Scanner *scanner, const char *source);

/**
 * @brief Produces next token from source string pointed to by scanner.
 */
Token scanToken(Scanner *scanner);

#endif // clox_scanner_h
