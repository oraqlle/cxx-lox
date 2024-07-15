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
 * @brief Initializes Scanner.
 */
void initScanner(Scanner *scanner, const char *source);

#endif // clox_scanner_h
