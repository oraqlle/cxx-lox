#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif // DEBUG_LOG_GC

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(VM *vm, Compiler *compiler, void *pointer, size_t oldSize,
                 size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(vm, compiler);
#else
        if (vm->bytesAllocated > vm->nextGC) {
            collectGarbage(vm, compiler);
        }
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

void markObject(VM *vm, Obj *object) {
    if (object == NULL) {
        return;
    }

    if (object->isMarked) {
        return;
    }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif // DEBUG_LOG_GC

    object->isMarked = true;

    if (vm->greyCapacity < vm->greyCount + 1) {
        vm->greyCapacity = GROW_CAPACITY(vm->greyCapacity);
        vm->greyStack = (Obj **)realloc(vm->greyStack, sizeof(Obj *) * vm->greyCapacity);

        if (vm->greyStack == NULL) {
            exit(1);
        }
    }

    vm->greyStack[vm->greyCount++] = object;
}

void markValue(VM *vm, Value value) {
    if (IS_OBJ(value)) {
        markObject(vm, AS_OBJ(value));
    }
}

static void markArray(VM *vm, ValueArray *array) {
    for (size_t idx = 0; idx < array->count; idx++) {
        markValue(vm, array->values[idx]);
    }
}

static void blackenObject(VM *vm, Obj *object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void *)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif // DEBUG_LOG_GC

    switch (object->type) {
        case OBJ_CLASS: {
            ObjClass *klass = (ObjClass *)object;
            markObject(vm, (Obj *)klass->name);
            markTable(vm, &klass->methods);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *)object;
            markObject(vm, (Obj *)closure->func);

            for (size_t idx = 0; idx < closure->upvalueCount; idx++) {
                markObject(vm, (Obj *)closure->upvalues[idx]);
            }

            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *func = (ObjFunction *)object;
            markObject(vm, (Obj *)func->name);
            markArray(vm, &func->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *)object;
            markObject(vm, (Obj *)instance->klass);
            markTable(vm, &instance->fields);
            break;
        }
        case OBJ_UPVALUE:
            markValue(vm, ((ObjUpvalue *)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void freeObject(VM *vm, Compiler *compiler, Obj *object) {

#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void *)object, object->type);
#endif // DEBUG_LOG_GC

    switch (object->type) {
        case OBJ_CLASS: {
            ObjClass *klass = (ObjClass *)object;
            freeTable(vm, compiler, &klass->methods);
            FREE(vm, compiler, ObjClass, object);
            break;
        }
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
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *)object;
            freeTable(vm, compiler, &instance->fields);
            FREE(vm, compiler, ObjInstance, object);
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
        markValue(vm, *slot);
    }

    for (size_t idx = 0; idx < vm->frameCount; idx++) {
        markObject(vm, (Obj *)vm->frames[idx].closure);
    }

    for (ObjUpvalue *upvalue = vm->openUpvalues; upvalue != NULL;
         upvalue = (ObjUpvalue *)upvalue->next) {
        markObject(vm, (Obj *)upvalue);
    }

    markTable(vm, &vm->globals);
    markCompilerRoots(vm, compiler);
}

static void traceReferences(VM *vm) {
    while (vm->greyCount > 0) {
        Obj *object = vm->greyStack[--vm->greyCount];
        blackenObject(vm, object);
    }
}

static void sweep(VM *vm, Compiler *compiler) {
    Obj *prev = NULL;
    Obj *object = vm->objects;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            prev = object;
            object = object->next;
        } else {
            Obj *unreached = object;
            object = object->next;

            if (prev != NULL) {
                prev->next = object;
            } else {
                vm->objects = object;
            }

            freeObject(vm, compiler, unreached);
        }
    }
}

void collectGarbage(VM *vm, Compiler *compiler) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm->bytesAllocated;
#endif // DEBUG_LOG_GC

    markRoots(vm, compiler);
    traceReferences(vm);
    tableRemoveWhite(vm, compiler, &vm->strings);
    sweep(vm, compiler);

    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next GC at %zu\n",
           before - vm->bytesAllocated, before, vm->bytesAllocated, vm->nextGC);
#endif // DEBUG_LOG_GC
}

void freeObjects(VM *vm, Compiler *compiler) {
    Obj *object = vm->objects;

    while (object != NULL) {
        Obj *next = object->next;
        freeObject(vm, compiler, object);
        object = next;
    }

    free((void *)vm->greyStack);
}
