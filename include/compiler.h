/**
 * @brief Bytecode compiler
 *
 * @file compiler.h
 */

#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "common.h"
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
 * @brief Local variables
 */
typedef struct {
    Token name;
    intmax_t depth;
} Local;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} FunctionType;

/**
 * @brief Compilers representation of the VM stack
 */
typedef struct {
    ObjFunction *func;
    FunctionType ftype;

    Local locals[UINT8_COUNT];
    intmax_t localCount;
    intmax_t scopeDepth;
} Compiler;

/**
 * @brief Function type for functions in parsing lookup table.
 */
typedef void (*ParseFn)(Parser *, Scanner *, VM *, Compiler *, bool);

/**
 * @brief Parsing rule.
 */
typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

/**
 * @brief Initializes compiler
 */
void initCompiler(Compiler *compiler, FunctionType ftype, VM *vm);

/**
 * @brief Compiles string of Lox source into bytecode.
 */
ObjFunction *compile(Scanner *scanner, const char *source, VM *vm);

#endif // clox_compiler_h
