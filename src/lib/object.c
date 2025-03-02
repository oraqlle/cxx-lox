#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm, compiler, type, objectType)                                     \
    (type *)allocateObject(vm, compiler, sizeof(type), objectType)

static void *allocateObject(VM *vm, Compiler *compiler, size_t size, ObjType type) {
    Obj *object = (Obj *)reallocate(vm, compiler, NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm->objects;
    vm->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d", (void *)object, size, type);
#endif // DEBUG_LOG_GC

    return object;
}

static ObjString *allocateString(VM *vm, Compiler *compiler, size_t length, char *chars,
                                 uint32_t hash) {
    ObjString *string = ALLOCATE_OBJ(vm, compiler, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    // Push-pop of value is done so that value is reachable
    // by VM and thus isn't swept if the GC is triggered by
    // `tableSet'.
    push(vm, OBJ_VAL(string));
    tableSet(vm, compiler, &vm->strings, string, NIL_VAL);
    pop(vm);

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

ObjFunction *newFunction(VM *vm, Compiler *compiler) {
    ObjFunction *func = ALLOCATE_OBJ(vm, compiler, ObjFunction, OBJ_FUNCTION);

    func->arity = 0;
    func->upvalueCount = 0;
    func->name = NULL;
    initChunk(&func->chunk);

    return func;
}

ObjInstance *newInstance(VM *vm, Compiler *compiler, ObjClass *klass) {
    ObjInstance *instance = ALLOCATE_OBJ(vm, compiler, ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjClass *newClass(VM *vm, Compiler *compiler, ObjString *name) {
    ObjClass *klass = ALLOCATE_OBJ(vm, compiler, ObjClass, OBJ_CLASS);
    klass->name = name;
    return klass;
}

ObjClosure *newClosure(VM *vm, Compiler *compiler, ObjFunction *func) {
    ObjUpvalue **upvalues = ALLOCATE(vm, compiler, ObjUpvalue *, func->upvalueCount);

    for (size_t idx = 0; idx < func->upvalueCount; idx++) {
        upvalues[idx] = NULL;
    }

    ObjClosure *closure = ALLOCATE_OBJ(vm, compiler, ObjClosure, OBJ_CLOSURE);
    closure->func = func;
    closure->upvalues = upvalues;
    closure->upvalueCount = func->upvalueCount;

    return closure;
}

ObjNative *newNative(VM *vm, Compiler *compiler, NativeFn func, uint8_t arity) {
    ObjNative *native = ALLOCATE_OBJ(vm, compiler, ObjNative, OBJ_NATIVE);
    native->arity = arity;
    native->func = func;
    return native;
}

ObjString *takeString(VM *vm, Compiler *compiler, size_t length, char *chars) {
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm->strings, chars, length, hash);

    if (interned != NULL) {
        FREE_ARRAY(vm, compiler, char, chars, length + 1);
        return interned;
    }

    return allocateString(vm, compiler, length, chars, hash);
}

ObjString *copyString(VM *vm, Compiler *compiler, size_t length, const char *chars) {
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm->strings, chars, length, hash);

    if (interned != NULL) {
        return interned;
    }

    char *heapChars = ALLOCATE(vm, compiler, char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(vm, compiler, length, heapChars, hash);
}

ObjUpvalue *newUpvalue(VM *vm, Compiler *compiler, Value *slot) {
    ObjUpvalue *upvalue = ALLOCATE_OBJ(vm, compiler, ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NIL_VAL;
    upvalue->next = NULL;
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
        case OBJ_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->func);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
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
