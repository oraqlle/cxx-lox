#include "table.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdint.h>
#include <string.h>

#define TABLE_MAX_LOAD 0.75

void initTable(Table *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(VM *vm, Compiler *compiler, Table *table) {
    FREE_ARRAY(vm, compiler, Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry *findEntry(Entry *entries, size_t capacity, ObjString *key) {
    size_t index = key->hash % capacity;
    Entry *tombstone = NULL;

    for (;;) {
        Entry *entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Empty entry
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) {
                    tombstone = entry;
                }
            }
        } else if (entry->key == key) {
            // Found entry
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

static void adjustCapacity(VM *vm, Compiler *compiler, Table *table, uint32_t capacity) {
    Entry *entries = ALLOCATE(vm, compiler, Entry, capacity);

    for (size_t i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (size_t i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];

        if (entry->key == NULL) {
            continue;
        }

        Entry *dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count += 1;
    }

    FREE_ARRAY(vm, compiler, Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(VM *vm, Compiler *compiler, Table *table, ObjString *key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        uint32_t capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(vm, compiler, table, capacity);
    }

    Entry *entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;

    if (isNewKey) {
        table->count += 1;
    }

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

void tableAddAll(VM *vm, Compiler *compiler, Table *from, Table *to) {
    for (size_t i = 0; i < from->capacity; i++) {
        Entry *entry = &from->entries[i];

        if (entry->key != NULL) {
            tableSet(vm, compiler, to, entry->key, entry->value);
        }
    }
}

bool tableGet(Table *table, ObjString *key, Value *value) {
    if (table->count == 0) {
        return false;
    }

    Entry *entry = findEntry(table->entries, table->capacity, key);

    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;
    return true;
}

bool tableDelete(Table *table, ObjString *key) {
    if (table->count == 0) {
        return false;
    }

    Entry *entry = findEntry(table->entries, table->capacity, key);

    if (entry->key == NULL) {
        return false;
    }

    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

ObjString *tableFindString(Table *table, const char *chars, size_t length,
                           uint32_t hash) {
    if (table->count == 0) {
        return NULL;
    }

    uint32_t index = hash % table->capacity;

    for (;;) {
        Entry *entry = &table->entries[index];

        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return NULL;
            }
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

void markTable(Table *table) {
    for (size_t idx = 0; idx < table->capacity; idx++) {
        Entry *entry = &table->entries[idx];
        markObject((Obj *)entry->key);
        markValue(entry->value);
    }
}
