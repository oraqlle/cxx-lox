#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

/**
 * @brief Obtains the type tag of an object
 */
#define OBJ_TYPE(object) (AS_OBJ(object)->type)

/**
 * @brief Checks if an object is a string
 */
#define IS_STRING(value) isObjType(value, OBJ_STRING)

/**
 * @brief Helper macros for extracting Lox strings and string data
 */
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

/**
 * @brief Type of heap object
 */
typedef enum {
    OBJ_STRING,
} ObjType;

/**
 * @brief Heap allocated objects in Lox
 */
struct Obj {
    ObjType type;
};

struct ObjString {
    Obj obj;
    size_t length;
    char *chars;
};

/**
 * @brief Takes ownership of raw char data it is passed
 */
ObjString *takeString(char *chars, size_t length);

/**
 * @brief Copies a string literal from scanned texted into string object
 */
ObjString *copyString(const char *chars, size_t length);

/**
 * @brief Helper function for displaying objects.
 */
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif // clox_object_h
