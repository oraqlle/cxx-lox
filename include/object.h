#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>

/**
 * @brief Obtains the type tag of an object
 */
#define OBJ_TYPE(object) (AS_OBJ(object)->type)

/**
 * @brief Checks if a value is a function object
 */
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)

/**
 * @brief Checks if a value is a string
 */
#define IS_STRING(value) isObjType(value, OBJ_STRING)

/**
 * @brief Helper macro for casting value to function object
 */
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))

/**
 * @brief Helper macros for extracting Lox strings and string data
 */
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

/**
 * @brief Type of heap object
 */
typedef enum {
    OBJ_FUNCTION,
    OBJ_STRING,
} ObjType;

/**
 * @brief Heap allocated objects in Lox
 */
struct Obj {
    ObjType type;
    struct Obj *next;
};

/**
 * @brief Function object type with it's own bytecode chunk
 */
typedef struct {
    Obj obj;
    uint8_t arity;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

/**
 * @brief Lox internal representation of strings
 */
struct ObjString {
    Obj obj;
    size_t length;
    char *chars;
    uint32_t hash;
};

/**
 * @brief Constructs a function object
 */
ObjFunction *newFunction(VM *vm);

/**
 * @brief Takes ownership of raw char data it is passed
 */
ObjString *takeString(size_t length, char *chars, VM *vm);

/**
 * @brief Copies a string literal from scanned texted into string object
 */
ObjString *copyString(size_t length, const char *chars, VM *vm);

/**
 * @brief Helper function for displaying objects.
 */
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif // clox_object_h
