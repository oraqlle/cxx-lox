/**
 * @brief Bytecode compiler
 *
 * @file compiler.h
 */

#ifndef clox_compiler_h
#define clox_compiler_h

#include "scanner.h"
#include "vm.h"

/**
 * @brief Compiles string of Lox source into bytecode.
 */
void compile(VM *vm, Scanner *scanner, const char *source);

#endif // clox_compiler_h
