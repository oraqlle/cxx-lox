/**
 * @brief VM structures and facilities
 *
 * @file vm.h
 */

#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "common.h"
#include "value.h"

#define STACK_MAX 256

/**
 * @brief VM structure.
 */
typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    Value stack[STACK_MAX];
    Value *stackTop;
} VM;

/**
 * @brief Return codes for VM.
 */
typedef enum {
    INTERPRETER_OK,
    INTERPRETER_COMPILE_ERR,
    INTERPRETER_RUNTIME_ERR
} InterpreterResult;

/**
 * @brief Initialize VM instance.
 */
void initVM(VM *vm);

/**
 * @brief Cleans up VM instance.
 */
void freeVM(VM *vm);

/**
 * @brief Interprets a chunk of bytecode.
 */
InterpreterResult interpret(VM *vm, Chunk *chunk);

/**
 * @brief Push to VM stack.
 */
void push(VM *vm, Value value);

/**
 * @brief Pop top value from VM stack.
 */
Value pop(VM *vm);

#endif // clox_vm_h
