/** @brief Bytecode representation of Lox
 *
 * @file chunk.h
 */

#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

/**
 * @brief Bytecode opcode values.
 */
typedef enum {
    OP_RETURN,
} OpCode;

/**
 * @brief Stores series of opcode in a chunk in the form of a dynamic array.
 */
typedef struct {
    size_t count;
    size_t capacity;
    uint8_t *code;
} Chunk;

/**
 * @brief Initialize Chunk.
 */
void initChunk(Chunk *chunk);

/**
 * @brief Append new opcode byte to `Chunk`.
 */
void writeChunk(Chunk *chunk, uint8_t byte);

/**
 * @brief Frees `Chunk`.
 */
void freeChunk(Chunk *chunk);

#endif // clox_chunk_h
