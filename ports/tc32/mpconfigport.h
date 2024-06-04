#include <stdint.h>

// options to control how MicroPython is built

// Use the minimal starting configuration (disables all optional features).
#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES)

// You can disable the built-in MicroPython compiler by setting the following
// config option to 0.  If you do this then you won't get a REPL prompt, but you
// will still be able to execute pre-compiled scripts, compiled with mpy-cross.
#define MICROPY_ENABLE_COMPILER (1)

#define MICROPY_QSTR_EXTRA_POOL mp_qstr_frozen_const_pool
#define MICROPY_ENABLE_GC (1)
#define MICROPY_HELPER_REPL (1)
#define MICROPY_ENABLE_EXTERNAL_IMPORT (1)
#define MICROPY_DEBUG_VERBOSE (0)
#define MICROPY_DEBUG_PRINTERS (0)
#define MICROPY_ENABLE_FINALISER (1)
#define MICROPY_LONGINT_IMPL (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_STACKLESS (1)

// This doesn't work. Maybe Telink's floating point emulation is broken.
#define MICROPY_FLOAT_IMPL (MICROPY_FLOAT_IMPL_FLOAT)

#define MICROPY_PY_MACHINE (1)
#define MICROPY_PY_MACHINE_INCLUDEFILE "ports/tc32/modmachine.c"
#define MICROPY_PY_MACHINE_PIN_MAKE_NEW mp_pin_make_new

#define MICROPY_VFS (1)
#define MICROPY_VFS_LFS1 (1)
#define MICROPY_HW_FLASH_MOUNT_AT_BOOT (1)

#define MICROPY_PY_ARRAY (1)
#define MICROPY_PY_BINASCII (1)
#define MICROPY_PY_BUILTINS_BYTES_HEX (1)
#define MICROPY_PY_IO (1)
#define MICROPY_PY_OS (1)
#define MICROPY_PY_OS_SYNC (1)
#define MICROPY_PY_PLATFORM (1)
#define MICROPY_PY_RANDOM (1)
#define MICROPY_PY_RANDOM_EXTRA_FUNCS (1)
#define MICROPY_PY_SYS_STDFILES (1)
#define MICROPY_PY_TIME (1)

#define MICROPY_ALLOC_PATH_MAX (256)

// Use the minimum headroom in the chunk allocator for parse nodes.
#define MICROPY_ALLOC_PARSE_CHUNK_INIT (16)

// type definitions for the specific machine

typedef intptr_t mp_int_t;   // must be pointer size
typedef uintptr_t mp_uint_t; // must be pointer size
typedef long mp_off_t;

// We need to provide a declaration/definition of alloca()
#include <alloca.h>

#define MICROPY_HW_BOARD_NAME "lt716"
#define MICROPY_HW_MCU_NAME "tc32"

#define MICROPY_MIN_USE_CORTEX_CPU (0)
#define MICROPY_MIN_USE_STM32_MCU (0)
#define MICROPY_HEAP_SIZE (2048) // heap size 2 kilobytes

#define MP_STATE_PORT MP_STATE_VM
