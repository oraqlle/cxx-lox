#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

/**
 * @brief Key-Value entry type for Lox's internal hash table data structure
 */
typedef struct {
    ObjString *key;
    Value value;
} Entry;

/**
 * @brief Hash table data structure
 */
typedef struct {
    size_t count;
    size_t capacity;
    Entry *entries;
} Table;

/**
 * @brief Initializes hash table
 */
void initTable(Table *table);

/**
 * @brief Destroys hash table
 */
void freeTable(Table *table);

/**
 * @brief Inserts or sets key entry with value.
 *
 * @returns true when entry is inserted and fals when updated
 */
bool tableSet(Table *table, ObjString *key, Value value);

#endif // clox_table_h
