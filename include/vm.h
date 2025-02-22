/**
 * @brief VM structures and facilities
 *
 * @file vm.h
 */

#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

/**
 * @brief VM structure.
 */
struct VM {
    CallFrame frames[FRAMES_MAX];
    size_t frameCount;

    Value stack[STACK_MAX];
    Value *stackTop;
    Table globals;
    Table strings;
    Obj *objects;
};

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
InterpreterResult interpret(VM *vm, Scanner *scanner, const char *source);

/**
 * @brief Push to VM stack.
 */
void push(VM *vm, Value value);

/**
 * @brief Pop top value from VM stack.
 */
Value pop(VM *vm);

#endif // clox_vm_h
