/**
 * @brief Memory management facilitis for internal types and VM
 *
 * @file memory.h
 */

#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "compiler.h"
#include "vm.h"

/**
 * @brief Growth algorithm for dynamic array types.
 *
 * @details Immediately bumps capacity value to 8 if previous capacity is less
 * than 8 to avoid repeated, small allocations early in a dynamic array's lifecycle.
 */
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

/**
 * @brief Frees Lox objects
 */
#define FREE(vm, compiler, type, pointer)                                                \
    reallocate(vm, compiler, pointer, sizeof(type), 0)

/**
 * @brief Grows a dynamic array using `reallocate()`.
 */
#define GROW_ARRAY(vm, compiler, type, pointer, oldCount, newCount)                      \
    (type *)reallocate(vm, compiler, pointer, sizeof(type) * (oldCount),                 \
                       sizeof(type) * (newCount))

/**
 * @brief Frees a dynamic array using `reallocate()`.
 */
#define FREE_ARRAY(vm, compiler, type, pointer, oldCount)                                \
    reallocate(vm, compiler, pointer, sizeof(type) * (oldCount), 0)

/**
 * @brief Allocates multiple bytes on VM
 */
#define ALLOCATE(vm, compiler, type, count)                                              \
    (type *)reallocate(vm, compiler, NULL, 0, sizeof(type) * (count))

/**
 * @brief Single heap memory management function for VM
 */
void *reallocate(VM *vm, Compiler *compiler, void *pointer, size_t oldSize,
                 size_t newSize);

/**
 * @brief Marks a Lox Obj to not be swept by GC
 */
void markObject(Obj *object);

/**
 * @brief Marks a Value to not be swept by GC
 */
void markValue(Value value);

/**
 * @brief Cleans up unused memory using mark-sweep GC
 */
void collectGarbage(VM *vm, Compiler *compiler);

/**
 * @brief Free heap objects from VM
 */
void freeObjects(VM *vm, Compiler *compiler);

#endif // clox_memory_h
