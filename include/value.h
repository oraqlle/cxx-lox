/**
 * @brief Types and structures for values in Lox
 *
 * @file value.h
 */

#ifndef clox_value_h
#define clox_value_h

#include "common.h"

/**
 * @brief Heap allocated objects in Lox
 */
typedef struct Obj Obj;

/**
 * @brief Builtin string type
 */
typedef struct ObjString ObjString;

/**
 * @brief Tags for Lox types
 */
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

/**
 * @brief Tagged union used to represent dynamic types of Lox
 */
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj *obj;
    } as;
} Value;

// clang-format off

/**
 * @brief Validates that dynamic type holds expected type (ie. has expected tag.)
 */
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

/**
 * @brief Extracts value from dynamic Lox type (tagged union)
 */
#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_OBJ(value)       ((value).as.obj)

/**
 * @brief Helper macros for constructing tagged union of a particular type.
 */
#define BOOL_VAL(value)     ((Value){VAL_BOOL, { .boolean = (value) }})
#define NIL_VAL             ((Value){VAL_NIL, { .number = 0 }})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, { .number = (value) }})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, { .obj = (Obj *)(object) }})

// clang-format on

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
 * @brief Checks if two Values are equal.
 */
bool valuesEqual(Value a, Value b);

/**
 * @brief Prints Value
 */
void printValue(Value value);

#endif // clox_value_h
