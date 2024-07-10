/**
 * @brief Memory management facilitis for internal types and VM
 *
 * @file memory.h
 */

#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

/**
 * @brief Growth algorithm for `Chunk`.
 *
 * @details Immediately bumps capacity value to 8 if previous capacity is less
 * than 8 to avoid repeated, small allocations early in a `Chunks` lifecycle.
 */
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

/**
 * @brief Makes `reallocate()` less cumbersome to use for growthing a `Chunk`.
 */
#define GROW_ARRAY(type, pointer, oldCount, newCount)                          \
    (type *)reallocate(pointer, sizeof(type) * (oldCount),                     \
                       sizeof(type) * (newCount))

/**
 * @brief Makes free operations with `reallocate()` less cumbersome for
 * `Chunks`.
 */
#define FREE_ARRAY(type, pointer, oldCount)                                    \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

/**
 * @brief Single heap memory management function for VM
 */
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#endif // clox_memory_h
