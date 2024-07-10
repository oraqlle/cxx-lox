/** @brief Bytecode representation of Lox
 *
 * @file chunk.h
 */

#ifndef clox_debug_h
#define clox_debug_h

#include "common.h"

#include "chunk.h"

/**
 * @brief Disassembles chunks
 */
void disassembleChunk(Chunk* chunk, const char* name);

/**
 * @brief Disassemble individual instruction
 */
size_t disassembleInstruction(Chunk *chunk, size_t offset);

#endif  // clox_debug_h

