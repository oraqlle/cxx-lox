#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}
