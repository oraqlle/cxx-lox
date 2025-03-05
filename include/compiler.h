/**
 * @brief Bytecode compiler
 *
 * @file compiler.h
 */

#ifndef clox_compiler_h
#define clox_compiler_h

#include "common.h"
#include "object.h"
#include "scanner.h"
#include <stdint.h>

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
    bool isCaptured;
} Local;

/**
 * @brief Represents variables closed over by closures
 */
typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_METHOD,
    TYPE_SCRIPT,
} FunctionType;

/**
 * @brief Compilers representation of the VM stack
 */
struct Compiler {
    Compiler *enclosing;

    ObjFunction *func;
    FunctionType ftype;

    Local locals[UINT8_COUNT];
    intmax_t localCount;
    Upvalue upvalues[UINT8_COUNT];
    intmax_t scopeDepth;
};

struct ClassCompiler {
    struct ClassCompiler *enclosing;
};

/**
 * @brief Function type for functions in parsing lookup table.
 */
typedef void (*ParseFn)(Parser *, Scanner *, VM *, Compiler *, ClassCompiler *, bool);

/**
 * @brief Parsing rule.student team member
 */
typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

/**
 * @brief Initializes compiler
 */
void initCompiler(Compiler *compiler, Compiler *enclosing, FunctionType ftype,
                  Parser *parser, VM *vm);

/**
 * @brief Compiles string of Lox source into bytecode.
 */
ObjFunction *compile(Scanner *scanner, const char *source, VM *vm);

/**
 * @brief Mark objects created by Compilers to not be swept by GC
 */
void markCompilerRoots(VM *vm, Compiler *compiler);

#endif // clox_compiler_h
