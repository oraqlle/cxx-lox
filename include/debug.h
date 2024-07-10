/**
 * @brief Disassembly tools
 *
 * @file debug.h
 */

#ifndef clox_debug_h
#define clox_debug_h

#include "common.h"

#include "chunk.h"

/**
 * @brief Disassembles bytecode `Chunks`.
 */
void disassembleChunk(Chunk *chunk, const char *name);

/**
 * @brief Disassembles an individual instruction
 */
size_t disassembleInstruction(Chunk *chunk, size_t offset);

#endif // clox_debug_h
