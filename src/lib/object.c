#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, vm, objectType)                                               \
    (type *)allocateObject(sizeof(type), vm, objectType)

static void *allocateObject(size_t size, VM *vm, ObjType type) {
    Obj *object = (Obj *)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm->objects;
    vm->objects = object;
    return object;
}

static ObjString *allocateString(size_t length, char *chars, uint32_t hash, VM *vm) {
    ObjString *string = ALLOCATE_OBJ(ObjString, vm, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm->strings, string, NIL_VAL);
    return string;
}

static uint32_t hashString(const char *key, size_t length) {
    uint32_t hash = 2166136261U;

    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)key[0];
        hash *= 16777619;
    }

    return hash;
}

ObjFunction *newFunction(VM *vm) {
    ObjFunction *func = ALLOCATE_OBJ(ObjFunction, vm, OBJ_FUNCTION);

    func->arity = 0;
    func->upvalueCount = 0;
    func->name = NULL;
    initChunk(&func->chunk);

    return func;
}

ObjClosure *newClosure(VM *vm, ObjFunction *func) {
    ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, vm, OBJ_CLOSURE);
    closure->func = func;
    return closure;
}

ObjNative *newNative(NativeFn func, uint8_t arity, VM *vm) {
    ObjNative *native = ALLOCATE_OBJ(ObjNative, vm, OBJ_NATIVE);
    native->arity = arity;
    native->func = func;
    return native;
}

ObjString *takeString(size_t length, char *chars, VM *vm) {
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm->strings, chars, length, hash);

    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(length, chars, hash, vm);
}

ObjString *copyString(size_t length, const char *chars, VM *vm) {
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm->strings, chars, length, hash);

    if (interned != NULL) {
        return interned;
    }

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(length, heapChars, hash, vm);
}

ObjUpvalue *newUpvalue(Value *slot, VM *vm) {
    ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, vm, OBJ_UPVALUE);
    upvalue->location = slot;
    return upvalue;
}

static void printFunction(ObjFunction *func) {
    if (func->name == NULL) {
        printf("<script>");
        return;
    }

    printf("<fn %s>", func->name->chars);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->func);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}
