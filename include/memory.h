/**
 * @brief Memory management facilitis for internal types and VM
 *
 * @file memory.h
 */

#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
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
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

/**
 * @brief Grows a dynamic array using `reallocate()`.
 */
#define GROW_ARRAY(type, pointer, oldCount, newCount)                                    \
    (type *)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

/**
 * @brief Frees a dynamic array using `reallocate()`.
 */
#define FREE_ARRAY(type, pointer, oldCount)                                              \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

/**
 * @brief Allocates multiple bytes on VM
 */
#define ALLOCATE(type, count) (type *)reallocate(NULL, 0, sizeof(type) * (count))

/**
 * @brief Single heap memory management function for VM
 */
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

/**
 * @brief Free heap objects from VM
 */
void freeObjects(VM *vm);

#endif // clox_memory_h
