/**
 * @brief Types and structures for values in Lox
 *
 * @file value.h
 */

#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include <string.h>

/**
 * @brief Heap allocated objects in Lox
 */
typedef struct Obj Obj;

/**
 * @brief Builtin string type
 */
typedef struct ObjString ObjString;

// clang-format off

#ifdef NAN_BOXING // Value type compressed into 64 bits

/**
 * @brief Quite NaN and iEEE-754 sign bit masks
 */
#define QNAN      ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT  ((uint64_t)0x8000000000000000)

#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.

typedef uint64_t  Value;

/**
 * @brief Validates that NaN-Boxed type holds expected type (correct bit tag)
 */
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define IS_OBJ(value)       (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

/**
 * @brief Extracts value from NaN-Boxed type 
 */
#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value)    valueToNum(value)
#define AS_OBJ(value)       ((Obj *)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

/**
 * @brief Helper macros for constructing a NaN-Boxed Value of a particular type
 */
#define BOOL_VAL(b)         ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL           ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL            ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL             ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num)     numToValue(num)
#define OBJ_VAL(obj)        (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(value));
    return num;
}

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else // Value type built as a tagged union


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

#endif // NAN_BOXING

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
void writeValueArray(VM *vm, Compiler *compiler, ValueArray *array, Value value);

/**
 * @brief Frees ValueArray.
 */
void freeValueArray(VM *vm, Compiler *compiler, ValueArray *array);

/**
 * @brief Checks if two Values are equal.
 */
bool valuesEqual(Value a, Value b);

/**
 * @brief Prints Value
 */
void printValue(Value value);

#endif // clox_value_h
