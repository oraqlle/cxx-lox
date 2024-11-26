#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

static void resetStack(VM *vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
}

static void runtimeError(VM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    CallFrame *frame = &vm->frames[vm->frameCount - 1];
    size_t instruction = (size_t)(frame->ip - frame->func->chunk.code - 1);
    size_t line = frame->func->chunk.lines[instruction];
    fprintf(stderr, "[line %zu] in script\n", line);
    resetStack(vm);
}

static Value peek(VM *vm, int distance) { return vm->stackTop[-1 - distance]; }

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(VM *vm) {
    ObjString *b = AS_STRING(pop(vm));
    ObjString *a = AS_STRING(pop(vm));

    size_t length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString *string = takeString(length, chars, vm);
    push(vm, OBJ_VAL(string));
}

void initVM(VM *vm) {
    resetStack(vm);
    vm->objects = NULL;

    initTable(&vm->globals);
    initTable(&vm->strings);
}

void freeVM(VM *vm) {
    freeObjects(vm);
    freeTable(&vm->globals);
    freeTable(&vm->strings);
}

static InterpreterResult run(VM *vm) {
    CallFrame *frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_CONSTANT() ((frame)->func->chunk.constants.values[READ_BYTE()])

#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op)                                                         \
    do {                                                                                 \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {                        \
            runtimeError(vm, "Operands must be numbers.");                               \
            return INTERPRETER_RUNTIME_ERR;                                              \
        }                                                                                \
        double b = AS_NUMBER(pop(vm));                                                   \
        double a = AS_NUMBER(pop(vm));                                                   \
        push(vm, valueType(a op b));                                                     \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");

        disassembleInstruction(&frame->func->chunk,
                               (size_t)(frame->ip - frame->func->chunk.code));
#endif

        uint8_t instruction;

        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_NIL:
                push(vm, NIL_VAL);
                break;
            case OP_TRUE:
                push(vm, BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(vm, BOOL_VAL(false));
                break;
            case OP_POP:
                pop(vm);
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->slots[slot]);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                Value value;

                if (!tableGet(&vm->globals, name, &value)) {
                    runtimeError(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRETER_RUNTIME_ERR;
                }

                push(vm, value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString *name = READ_STRING();
                tableSet(&vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();

                if (tableSet(&vm->globals, name, peek(vm, 0))) {
                    tableDelete(&vm->globals, name);
                    runtimeError(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRETER_RUNTIME_ERR;
                }

                break;
            }
            case OP_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_ADD: {
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenate(vm);
                } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(a + b));
                } else {
                    runtimeError(vm, "Operands must be two numbers or two strings.");
                    return INTERPRETER_RUNTIME_ERR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
            case OP_NOT:
                push(vm, BOOL_VAL(isFalsey(pop(vm))));
                break;
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(vm, 0))) {
                    runtimeError(vm, "Operand must be a number.");
                    return INTERPRETER_RUNTIME_ERR;
                }
                push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
                break;
            }
            case OP_PRINT: {
                printValue(pop(vm));
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();

                if (isFalsey(peek(vm, 0))) {
                    frame->ip += offset;
                }

                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_RETURN: {
                // Exit interpreter
                return INTERPRETER_OK;
            }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpreterResult interpret(VM *vm, Scanner *scanner, const char *source) {
    ObjFunction *func = compile(scanner, source, vm);

    if (func == NULL) {
        return INTERPRETER_COMPILE_ERR;
    }

    push(vm, OBJ_VAL(func));

    CallFrame *frame = &vm->frames[vm->frameCount++];
    frame->func = func;
    frame->ip = func->chunk.code;
    frame->slots = vm->stack;

    return run(vm);
}

void push(VM *vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}

Value pop(VM *vm) {
    vm->stackTop--;
    return *vm->stackTop;
}
