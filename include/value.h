/**
 * @brief Bytecode representation of Lox
 *
 * @file array.h
 */

#ifndef clox_value_h
#define clox_value_h

#include "common.h"

/**
 * @brief Type alias for numerics values.
 */
typedef double Value;

/**
 * @brief Dynamic array of values.
 */
typedef struct {
    uint8_t capacity;
    uint8_t count;
    Value *values;
} ValueArray;

/**
 * @brief Initialize ValueArray.
 */
void initValueArray(ValueArray *array);

/**
 * @brief Append new value to ValueArray.
 */
void writeValueArray(ValueArray *array, Value value);

/**
 * @brief Frees ValueArray.
 */
void freeValueArray(ValueArray *array);

/**
 * @brief Prints Value
 */
void printValue(Value value);

#endif // clox_value_h
