#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "object.h"
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
 * @returns true when entry is inserted and fails when updated
 */
bool tableSet(Table *table, ObjString *key, Value value);

/**
 * @brief Copies all entries from one hash table to another.
 */
void tableAddAll(Table *from, Table *to);

/**
 * @brief Extract pointer to entry i hash table
 *
 * @returns true if entry is found, false otherwise
 */
bool tableGet(Table *from, ObjString *key, Value *value);

/**
 * @brief Deletes and entry from hash table
 */
bool tableDelete(Table *table, ObjString *key);

#endif // clox_table_h
