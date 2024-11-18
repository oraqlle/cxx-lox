/**
 * @brief Bytecode compiler
 *
 * @file compiler.h
 */

#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "object.h"
#include "scanner.h"

/**
 * @brief Parser type.
 */
typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

/**
 * @brief Precedence of different expressions encoded by enum value.
 */
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
} Precedence;

/**
 * @brief Function type for functions in parsing lookup table.
 */
typedef void (*ParseFn)(Parser *, Scanner *, VM *);

/**
 * @brief Parsing rule.
 */
typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

/**
 * @brief Compiles string of Lox source into bytecode.
 */
bool compile(Scanner *scanner, const char *source, Chunk *chunk, VM *vm);

#endif // clox_compiler_h
