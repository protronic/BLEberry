/********************************************************************
** Berry configuration for BLEberry (Zephyr, STM32WBA).
**
** Based on the upstream default/berry_conf.h, trimmed for a
** flash/RAM constrained BLE SoC: no file system, no OS module,
** single-precision floats, 32-bit integers.
**
** NOTE: this header is also parsed by Berry's `coc` code generator,
** so keep it to plain preprocessor definitions.
********************************************************************/
#ifndef BERRY_CONF_H
#define BERRY_CONF_H

#include <assert.h>

/* Berry interpreter debug switch */
#ifndef BE_DEBUG
#define BE_DEBUG                        0
#endif

/* 0: int, 1: long, 2: long long -- 32-bit integers are plenty here */
#define BE_INTGER_TYPE                  1

/* single-precision floats to save RAM and cycles on Cortex-M33 */
#define BE_USE_SINGLE_FLOAT             1

/* maximum size of a bytes() object */
#define BE_BYTES_MAX_SIZE               (16 * 1024)

/* solidify built-in objects into flash (big RAM saving) */
#define BE_USE_PRECOMPILED_OBJECT       1

/* drop per-function source file names (saves RAM) */
#define BE_DEBUG_SOURCE_FILE            0

/* runtime line info as uint16_t (compact) */
#define BE_DEBUG_RUNTIME_INFO           2

/* no variable tracking debug info */
#define BE_DEBUG_VAR_INFO               0

/* no observability hooks */
#define BE_USE_PERF_COUNTERS            0
#define BE_VM_OBSERVABILITY_SAMPLING    20

/* VM stack limits (slots, not bytes) */
#define BE_STACK_TOTAL_MAX              2000
#define BE_STACK_FREE_MIN               10
#define BE_STACK_START                  25

#define BE_CONST_SEARCH_SIZE            50

#define BE_USE_STR_HASH_CACHE           0

/* no file system on this target */
#define BE_USE_FILE_SYSTEM              0

/* the REPL needs the compiler, obviously */
#define BE_USE_SCRIPT_COMPILER          1

/* bytecode save/load needs a file system */
#define BE_USE_BYTECODE_SAVER           0
#define BE_USE_BYTECODE_LOADER          0

#define BE_USE_SHARED_LIB               0

#define BE_USE_OVERLOAD_HASH            1

#define BE_USE_DEBUG_HOOK               0
#define BE_USE_DEBUG_GC                 0
#define BE_USE_DEBUG_STACK              0
#define BE_USE_MEM_ALIGNED              0

/* built-in modules: keep the OS-independent ones */
#define BE_USE_STRING_MODULE            1
#define BE_USE_JSON_MODULE              1
#define BE_USE_MATH_MODULE              1
#define BE_USE_TIME_MODULE              0
#define BE_USE_OS_MODULE                0
#define BE_USE_GLOBAL_MODULE            1
#define BE_USE_SYS_MODULE               0
#define BE_USE_DEBUG_MODULE             0
#define BE_USE_GC_MODULE                1
#define BE_USE_SOLIDIFY_MODULE          0
#define BE_USE_INTROSPECT_MODULE        1
#define BE_USE_STRICT_MODULE            1

/* memory allocation: libc malloc arena, sized via
 * CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE in prj.conf */
#define BE_EXPLICIT_ABORT               abort
#define BE_EXPLICIT_EXIT                exit
#define BE_EXPLICIT_MALLOC              malloc
#define BE_EXPLICIT_FREE                free
#define BE_EXPLICIT_REALLOC             realloc

#define be_assert(expr)                 assert(expr)

#endif
