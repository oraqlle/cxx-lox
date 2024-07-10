/** @brief Bytecode representation of Lox
 *
 * @file chunk.h
 */

#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "memory.h"

/**
 * @brief Bytecode opcode type
 */
typedef enum {
    OP_RETURN,
} OpCode;

/**
 * @brief Dynamic array of opcodes
 */
typedef struct {
    size_t count;
    size_t capacity;
    uint8_t* code;
} Chunk;

/**
 * @brief Initialize Chunk
 */
void initChunk(Chunk* chunk);

/**
 * @brief Push back new byte
 */
void writeChunk(Chunk* chunk, uint8_t byte);

/**
 * @brief Frees Chunk array
 */
void freeChunk(Chunk* chunk);

#endif  // clox_chunk_h

