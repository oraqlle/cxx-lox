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
    uint32_t count;
    uint32_t capacity;
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

/**
 * @brief Finds a particular string entry in a hash table, avoiding the use of findEntry()
 */
ObjString *tableFindString(Table *table, const char *chars, size_t length, uint32_t hash);

/**
 * @brief Marks globals in the VMs hash table to not be swept by GC
 */
void markTable(Table *table);

#endif // clox_table_h
