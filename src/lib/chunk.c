#include "chunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

void initChunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void writeChunk(VM *vm, Compiler *compiler, Chunk *chunk, uint8_t byte, size_t line) {
    if (chunk->capacity < chunk->count + 1) {
        size_t oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code =
            GROW_ARRAY(vm, compiler, uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines =
            GROW_ARRAY(vm, compiler, size_t, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

uint8_t addConstant(VM *vm, Compiler *compiler, Chunk *chunk, Value value) {
    // Push-pop of value is done so that value is reachable
    // by VM and thus isn't swept if the GC is triggered by
    // `writeValueArray'.
    push(vm, value);
    writeValueArray(vm, compiler, &chunk->constants, value);
    pop(vm);
    return chunk->constants.count - 1;
}

void freeChunk(VM *vm, Compiler *compiler, Chunk *chunk) {
    FREE_ARRAY(vm, compiler, uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(vm, compiler, size_t, chunk->lines, chunk->capacity);
    freeValueArray(vm, compiler, &chunk->constants);
    initChunk(chunk);
}
