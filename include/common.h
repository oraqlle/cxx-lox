
/** @brief Common components of VM
 *
 * @file common.h
 */

#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UINT8_COUNT (UINT8_MAX + 1)

// Forward declare VM type
typedef struct VM VM;

// Forward declare Compiler type
typedef struct Compiler Compiler;

// Forward declare ClassCompiler type
typedef struct ClassCompiler ClassCompiler;

#ifdef CLOX_DEVELOPER_MODE
#undef DEBUG_TRACE_EXECUTION
#define DEBUG_TRACE_EXECUTION

#undef DEBUG_PRINT_CODE
#define DEBUG_PRINT_CODE

#undef DEBUG_STRESS_GC
#define DEBUG_STRESS_GC

#undef DEBUG_LOG_GC
#define DEBUG_LOG_GC
#endif

#endif // clox_common_h
