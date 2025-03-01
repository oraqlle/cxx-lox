#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif // DEBUG_LOG_GC

void *reallocate(VM *vm, Compiler *compiler, void *pointer, size_t oldSize,
                 size_t newSize) {
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(vm, compiler);
#endif // DEBUG_STRESS_GC
    }
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);

    if (result == NULL) {
        exit(1);
    }

    return result;
}

void markObject(Obj *object) {
    if (object == NULL) {
        return;
    }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif // DEBUG_LOG_GC

    object->isMarked = true;
}

void markValue(Value value) {
    if (IS_OBJ(value)) {
        markObject(AS_OBJ(value));
    }
}

static void freeObject(VM *vm, Compiler *compiler, Obj *object) {

#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void *)object, object->type);
#endif // DEBUG_LOG_GC

    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *)object;
            FREE_ARRAY(vm, compiler, ObjUpvalue *, closure->upvalues,
                       closure->upvalueCount);
            FREE(vm, compiler, ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *func = (ObjFunction *)object;
            freeChunk(vm, compiler, &func->chunk);
            FREE(vm, compiler, ObjFunction, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(vm, compiler, ObjNative, object);
            break;
        }
        case OBJ_STRING: {
            ObjString *string = (ObjString *)object;
            FREE_ARRAY(vm, compiler, char, string->chars, string->length + 1);
            FREE(vm, compiler, ObjString, object);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(vm, compiler, ObjUpvalue, object);
            break;
        }
    }
}

static void markRoots(VM *vm, Compiler *compiler) {
    for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(*slot);
    }

    for (size_t idx = 0; idx < vm->frameCount; idx++) {
        markObject((Obj *)vm->frames[idx].closure);
    }

    for (ObjUpvalue *upvalue = vm->openUpvalues; upvalue != NULL;
         upvalue = (ObjUpvalue *)upvalue->next) {
        markObject((Obj *)upvalue);
    }

    markTable(&vm->globals);
    markCompilerRoots(vm, compiler);
}

void collectGarbage(VM *vm, Compiler *compiler) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif // DEBUG_LOG_GC

    markRoots(vm, compiler);

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
#endif // DEBUG_LOG_GC
}

void freeObjects(VM *vm, Compiler *compiler) {
    Obj *object = vm->objects;

    while (object != NULL) {
        Obj *next = object->next;
        freeObject(vm, compiler, object);
        object = next;
    }
}
