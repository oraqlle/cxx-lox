#include "common.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table *table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry *findEntry(Entry *entries, size_t capacity, ObjString *key) {
    size_t index = key->hash % capacity;

    for (;;) {
        Entry *entry = &entries[index];

        if (entry->key == key || entry->key == NULL) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool tableSet(Table *table, ObjString *key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        size_t capcity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capcity);
    }

    Entry *entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;

    if (isNewKey) { table->count += 1; }

    entry->key = key;
    entry->value = value;
    return isNewKey;
}
