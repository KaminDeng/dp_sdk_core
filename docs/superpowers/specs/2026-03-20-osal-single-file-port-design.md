# Design: osal Portable Interface — Single-File Port (`osal_port.h`)

**Date:** 2026-03-20
**Status:** Approved

---

## Problem

Porting the `osal` library to a new platform currently requires creating and maintaining
**four files** in a separate `osal_port` directory, with that directory path **hard-coded**
inside `osal/CMakeLists.txt` (line 7: `include("${CMAKE_CURRENT_SOURCE_DIR}/../osal_port/osal_config.cmake")`).

| File | Purpose |
|---|---|
| `osal_port_config.h` | Platform macros + `osal_port_debug_write` declaration |
| `osal_port_debug.cpp` | `osal_port_debug_write` implementation |
| `osal_config.cmake` | Backend selection + dependency library list |
| `osal_test_framework_config.h` | Per-test-case enable/disable flags |

**Consequences:**
- The `osal_port` directory name and location are fixed; moving it requires editing the osal library itself.
- Four separate files to understand, create, and synchronise.
- No default port — the library cannot compile without user-provided files.
- The test flag system (`TestOSALXxxEnabled`) is fragmented and verbose.

---

## Solution: Single-File Port + `OSAL_PORT_DIR` CMake Variable

### Goals

1. **Portability** — users create exactly **one file** (`osal_port.h`) to port osal to any platform.
2. **Zero-config default** — the library ships with a built-in POSIX port; it compiles and runs without any user configuration.
3. **Path independence** — the port directory can be anywhere; its location is passed via the CMake variable `OSAL_PORT_DIR`.
4. **Clean template** — `osal/port/template/osal_port.h` is the authoritative guide for porting.

---

## Architecture

### Directory layout after this change

```
osal/
├── CMakeLists.txt           ← Modified: reads OSAL_PORT_DIR, auto-selects backend
├── src/
│   ├── osal.h               ← Modified: includes osal_port.h (was osal_port_config.h)
│   ├── debug/
│   │   ├── osal_debug.h     ← Unchanged
│   │   └── osal_debug.cpp   ← Unchanged (still calls osal_port_debug_write)
│   ├── interface/           ← Unchanged (13 pure-virtual headers)
│   └── impl/
│       ├── posix/           ← Unchanged
│       └── cmsis_os/        ← Unchanged
├── test/
│   ├── osal_test_main.h     ← Unchanged
│   ├── osal_test_main.cpp   ← Minor: includes osal_port.h (not osal_test_framework_config.h)
│   └── gtest/
│       └── gtest_*.cpp      ← Modified: replace TestOSALXxxEnabled macros with new ones
└── port/
    ├── builtin_posix/
    │   └── osal_port.h      ← NEW: built-in POSIX port (zero-config default)
    └── template/
        └── osal_port.h      ← NEW: user copy-and-fill porting template
```

**User's port directory (any location, any name):**
```
my_project/
├── my_osal_port/
│   └── osal_port.h          ← User creates this ONE file
└── CMakeLists.txt
    └── set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_osal_port)
```

---

## Interface: `osal_port.h`

This is the **only file** a user needs to create. It is organised into five sections.
All sections are mandatory; defaults are provided via `#ifndef` where safe.

```cpp
// osal_port.h — Complete porting interface for the osal library.
// Copy from osal/port/template/osal_port.h and fill in your platform values.
#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Section 1: Backend Selection
// Uncomment EXACTLY ONE of the following lines.
// ═══════════════════════════════════════════════════════════════════════════
#define OSAL_BACKEND_POSIX
// #define OSAL_BACKEND_CMSIS_OS

// ═══════════════════════════════════════════════════════════════════════════
// Section 2: Platform Includes
// POSIX:    leave empty (osal uses std:: and pthread internally)
// CMSIS-OS: #include "cmsis_os2.h" (and any required platform headers)
// ═══════════════════════════════════════════════════════════════════════════
// #include "cmsis_os2.h"

// ═══════════════════════════════════════════════════════════════════════════
// Section 3: Platform Constants
// ═══════════════════════════════════════════════════════════════════════════

// Minimum thread stack size in bytes.
// POSIX: PTHREAD_STACK_MIN (typically 16384). CMSIS-OS: configMINIMAL_STACK_SIZE.
#ifndef OSAL_PORT_THREAD_MIN_STACK_SIZE
#  define OSAL_PORT_THREAD_MIN_STACK_SIZE  4096
#endif

// Default thread priority.
// POSIX: 0 (unused, POSIX does not use a numeric default priority here).
// CMSIS-OS: osPriorityNormal.
#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#  define OSAL_PORT_THREAD_DEFAULT_PRIORITY  0
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Section 4: Debug Output
//
// Implement osal_port_debug_write as an inline function.
// It is called by osal_debug.cpp for every log message.
//
// buf: null-terminated log string (may contain embedded newlines).
// len: number of bytes in buf (not including any trailing NUL).
//
// The function must be safe to call from task/thread context.
// ═══════════════════════════════════════════════════════════════════════════
#include <cstdint>

// Example implementations (uncomment/modify one):

// POSIX / Linux / macOS — write to stdout
#include <unistd.h>
inline void osal_port_debug_write(const char* buf, uint32_t len) {
    ::write(1, buf, static_cast<size_t>(len));
}

// STM32 HAL UART:
// #include "usart.h"
// inline void osal_port_debug_write(const char* buf, uint32_t len) {
//     HAL_UART_Transmit(&huart6, (const uint8_t*)buf, len, HAL_MAX_DELAY);
// }

// Zephyr:
// #include <zephyr/sys/printk.h>
// inline void osal_port_debug_write(const char* buf, uint32_t len) {
//     printk("%.*s", static_cast<int>(len), buf);
// }

// ═══════════════════════════════════════════════════════════════════════════
// Section 5: Test Feature Flags
//
// OSAL_TEST_ALL = 1  → enable every test suite (recommended for first bring-up)
// OSAL_TEST_ALL = 0  → use the per-component flags below for fine control
// ═══════════════════════════════════════════════════════════════════════════
#define OSAL_TEST_ALL 1

#if !OSAL_TEST_ALL
// Per-component test enable flags.
// Set each to 1 (enable) or 0 (skip/GTEST_SKIP).
#  define OSAL_TEST_THREAD_ENABLED              1
#  define OSAL_TEST_CHRONO_ENABLED              1
#  define OSAL_TEST_MUTEX_ENABLED               1
#  define OSAL_TEST_CONDITION_VARIABLE_ENABLED  1
#  define OSAL_TEST_LOCKGUARD_ENABLED           1
#  define OSAL_TEST_MEMORY_MANAGER_ENABLED      1
#  define OSAL_TEST_QUEUE_ENABLED               1
#  define OSAL_TEST_SEMAPHORE_ENABLED           1
#  define OSAL_TEST_SPINLOCK_ENABLED            1
#  define OSAL_TEST_RWLOCK_ENABLED              1
#  define OSAL_TEST_TIMER_ENABLED               1
#  define OSAL_TEST_THREAD_POOL_ENABLED         1
#endif
```

### Resolved macros

`osal/CMakeLists.txt` propagates a set of standardised macros to all consumers so that
the osal library internals (especially `impl/posix/include/osal_thread.h` and
`impl/cmsis_os/include/osal_thread.h`) can reference constants without knowing the
platform. The internal impl files are updated to use these macro names:

| Old name (to be replaced) | New name (from `osal_port.h`) |
|---|---|
| `OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE` | `OSAL_PORT_THREAD_MIN_STACK_SIZE` |
| `OSAL_CONFIG_THREAD_DEFAULT_PRIORITY` | `OSAL_PORT_THREAD_DEFAULT_PRIORITY` |

---

## CMake Interface

### `osal/CMakeLists.txt` — key changes

```cmake
cmake_minimum_required(VERSION 3.15)

# ── Port directory resolution ──────────────────────────────────────────────
# Users set OSAL_PORT_DIR before add_subdirectory(osal) to supply their port.
# If not set, the built-in POSIX port is used.
if (NOT DEFINED OSAL_PORT_DIR)
    message(STATUS "[osal] OSAL_PORT_DIR not set — using built-in POSIX port")
    set(OSAL_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port/builtin_posix")
endif()

if (NOT EXISTS "${OSAL_PORT_DIR}/osal_port.h")
    message(FATAL_ERROR
        "[osal] osal_port.h not found in OSAL_PORT_DIR='${OSAL_PORT_DIR}'.\n"
        "Copy osal/port/template/osal_port.h to your port directory and fill it in.")
endif()

# ── Backend auto-detection from osal_port.h ───────────────────────────────
# Scan the header for the backend selector macro.
file(STRINGS "${OSAL_PORT_DIR}/osal_port.h" _port_lines)
set(_osal_backend "posix")  # default
foreach(_line IN LISTS _port_lines)
    if (_line MATCHES "^[[:space:]]*#[[:space:]]*define[[:space:]]+OSAL_BACKEND_CMSIS_OS")
        set(_osal_backend "cmsis_os")
        break()
    endif()
endforeach()
message(STATUS "[osal] Backend: ${_osal_backend}")

# ── Source selection ───────────────────────────────────────────────────────
file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${OSAL_IMPL_PATH}/${_osal_backend}/*.cpp")
file(GLOB_RECURSE GTEST_SUITE_SOURCES "${GOOGLETEST_SUITE_PATH}/*.cpp")

# ── Library target ─────────────────────────────────────────────────────────
add_library(osal STATIC
    src/debug/osal_debug.cpp
    ${SYSTEM_IMPL_SOURCES}
    test/osal_test_main.cpp
    ${GTEST_SUITE_SOURCES}
)

# ── Include directories ────────────────────────────────────────────────────
# OSAL_PORT_DIR is PUBLIC so that downstream targets can also include osal_port.h
# (needed if they instantiate osal objects directly).
target_include_directories(osal
    PUBLIC
        "${OSAL_PORT_DIR}"       # osal_port.h (user port or built-in)
        src
        src/interface
        src/debug
        $<IF:$<STREQUAL:${_osal_backend},cmsis_os>,src/impl/cmsis_os/include,>
        $<IF:$<STREQUAL:${_osal_backend},posix>,src/impl/posix/include,>
        test
        ${GOOGLETEST_SUITE_PATH}
)

# ── Dependencies ───────────────────────────────────────────────────────────
# gtest is the only external dependency for the POSIX built-in case.
# For user ports that depend on additional libraries, they must link them
# externally via target_link_libraries(osal PUBLIC <their-libs>) in their CMake.
target_link_libraries(osal gtest)
```

### User integration (two lines)

```cmake
# In the user's CMakeLists.txt, before add_subdirectory(osal):
set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_osal_port)  # path to osal_port.h
add_subdirectory(path/to/osal)
```

### Zero-config (built-in POSIX port)

If `OSAL_PORT_DIR` is **not set**, the library uses `osal/port/builtin_posix/osal_port.h`
which selects the POSIX backend and writes debug output to stdout.
No user action required.

---

## `src/osal.h` Update

```cpp
// src/osal.h
#pragma once

#if __has_include("osal_port.h")
#  include "osal_port.h"
#else
#  error "[osal] osal_port.h not found. " \
         "Set OSAL_PORT_DIR in CMake before add_subdirectory(osal). " \
         "Template: copy osal/port/template/osal_port.h to your port directory."
#endif
```

---

## Internal Impl File Updates

Two constant names change throughout `src/impl/`:

| File | Old | New |
|---|---|---|
| `posix/include/osal_thread.h:48-50` | `OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE` | `OSAL_PORT_THREAD_MIN_STACK_SIZE` |
| `cmsis_os/include/osal_thread.h:38` | `OSAL_CONFIG_THREAD_DEFAULT_PRIORITY` | `OSAL_PORT_THREAD_DEFAULT_PRIORITY` |

All other impl code is unchanged.

---

## Test File Updates

### `gtest_*.cpp` macro replacement

Old (per-test granularity, 60+ macros spread across 12 files):
```cpp
#if (TestOSALThreadStartEnabled)
```

New (per-component granularity, derived from `OSAL_TEST_ALL` or per-component flag):
```cpp
// Resolved at the top of each gtest_*.cpp from osal_port.h values:
// OSAL_TEST_ALL=1 → component flag = 1
// OSAL_TEST_ALL=0 → component flag = whatever OSAL_TEST_THREAD_ENABLED is set to
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
```

In practice, each `gtest_*.cpp` replaces its test guard macro:
- `gtest_thread.cpp`: all `#if (TestOSALThreadXxxEnabled)` → `#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)`
- `gtest_mutex.cpp`: → `#if (OSAL_TEST_MUTEX_ENABLED || OSAL_TEST_ALL)`
- ... (same pattern for all 12 files)

### `test/osal_test_main.cpp`

Remove the `#include "osal_test_framework_config.h"` line. The test flags now come from
`osal_port.h` (which is already included transitively via `osal_debug.h` → `osal.h`).

---

## Test Feature Flag Compatibility

Old fine-grained per-test flags (`TestOSALThreadStartEnabled`, etc.) are **removed**.
New flags are per-component (`OSAL_TEST_THREAD_ENABLED`), giving 12 flags instead of 60+.

If a platform needs to disable a specific test case within a component (e.g., skip
`TestOSALThreadSetAndGetPriority` on a platform without `SCHED_FIFO` support), that can
be done by wrapping the specific test with a conditional inside the test body (`GTEST_SKIP`
based on a runtime platform check), rather than a compile-time macro.

---

## Built-in POSIX Port (`osal/port/builtin_posix/osal_port.h`)

```cpp
// osal/port/builtin_posix/osal_port.h
// Built-in POSIX port. Used when OSAL_PORT_DIR is not set.
// Suitable for Linux, macOS, and any POSIX-compliant environment.
#pragma once

#define OSAL_BACKEND_POSIX

#include <cstdint>
#include <unistd.h>

#define OSAL_PORT_THREAD_MIN_STACK_SIZE    4096
#define OSAL_PORT_THREAD_DEFAULT_PRIORITY  0

inline void osal_port_debug_write(const char* buf, uint32_t len) {
    ::write(1, buf, static_cast<size_t>(len));
}

#define OSAL_TEST_ALL 1
```

---

## File Change Summary

| File | Action | Notes |
|---|---|---|
| `CMakeLists.txt` | **Rewrite** | Remove `include(../osal_port/osal_config.cmake)`; add `OSAL_PORT_DIR` logic; auto-detect backend |
| `src/osal.h` | **Modify** | Include `osal_port.h` instead of `osal_port_config.h` |
| `src/impl/posix/include/osal_thread.h` | **Modify** | Rename constant macros; add `#include "osal.h"` so `OSAL_PORT_THREAD_MIN_STACK_SIZE` resolves (see POSIX include chain note below) |
| `src/impl/cmsis_os/include/osal_thread.h` | **Modify** | Rename constant macros (already includes `osal.h` → `osal_port.h`, no extra include needed) |
| `test/osal_test_main.cpp` | **Modify** | Remove `#include "osal_test_framework_config.h"` |
| `test/gtest/gtest_chrono.cpp` | **Modify** | Replace `TestOSALChronoXxxEnabled` with `OSAL_TEST_CHRONO_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_condition_variable.cpp` | **Modify** | Replace with `OSAL_TEST_CONDITION_VARIABLE_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_lockguard.cpp` | **Modify** | Replace with `OSAL_TEST_LOCKGUARD_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_memory_manger.cpp` | **Modify** | Replace with `OSAL_TEST_MEMORY_MANAGER_ENABLED \|\| OSAL_TEST_ALL` (note: filename typo `manger` is intentional — do not rename) |
| `test/gtest/gtest_mutex.cpp` | **Modify** | Replace with `OSAL_TEST_MUTEX_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_queue.cpp` | **Modify** | Replace with `OSAL_TEST_QUEUE_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_rwlock.cpp` | **Modify** | Replace with `OSAL_TEST_RWLOCK_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_semaphore.cpp` | **Modify** | Replace with `OSAL_TEST_SEMAPHORE_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_spin_lock.cpp` | **Modify** | Replace with `OSAL_TEST_SPINLOCK_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_thread.cpp` | **Modify** | Replace with `OSAL_TEST_THREAD_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_thread_pool.cpp` | **Modify** | Replace with `OSAL_TEST_THREAD_POOL_ENABLED \|\| OSAL_TEST_ALL` |
| `test/gtest/gtest_timer.cpp` | **Modify** | Replace with `OSAL_TEST_TIMER_ENABLED \|\| OSAL_TEST_ALL` |
| `port/builtin_posix/osal_port.h` | **New** | Built-in POSIX port |
| `port/template/osal_port.h` | **New** | User porting template with full documentation |
| `osal_port/osal_port_config.h` | **Deleted** (from `applications/osal_port/`) | Replaced by `osal_port.h` |
| `osal_port/osal_port_debug.cpp` | **Deleted** | `osal_port_debug_write` is now inline in `osal_port.h` |
| `osal_port/osal_config.cmake` | **Deleted** | Backend selection moved to `osal_port.h` Section 1 |
| `osal_port/osal_test_framework_config.h` | **Deleted** | Test flags moved to `osal_port.h` Section 5 |
| `osal_port/CMakeLists.txt` | **Deleted** | osal_port is no longer a separate CMake target |
| `osal_port/cmsis_os2_freertos/` | **Out of scope — unchanged** | CMSIS-OS adapter sources; used by the host project, not by osal directly |
| All other `src/` and `test/` files | **Unchanged** | Interface, impl logic, gtest test bodies |

### POSIX include chain note

The POSIX `osal_thread.h` currently does **not** include `osal.h` (unlike the CMSIS-OS version).
It references `OSAL_PORT_THREAD_MIN_STACK_SIZE` (formerly `OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE`),
which must now come from `osal_port.h`. Adding `#include "osal.h"` at the top of
`posix/include/osal_thread.h` resolves the macro via the chain:
`osal_thread.h` → `osal.h` → `osal_port.h`.

The same fix is **not needed** for `cmsis_os/include/osal_thread.h` (already includes `osal.h`).

### `osal_port_debug_write` signature change

The old signature: `void osal_port_debug_write(char* buf, uint32_t len)` (non-const).
The new signature: `inline void osal_port_debug_write(const char* buf, uint32_t len)` (const).

The call site in `osal_debug.cpp` passes a `char buf[512]` which is compatible with `const char*`.
**Implementors migrating an existing port must update the function signature to add `const`.**

---

## Constraints & Non-Goals

- **Breaking change is accepted**: existing `osal_port` directories must be migrated to the new `osal_port.h` format.
- **`osal_port_debug_write` must be `inline`**: `inline` is the correct C++ mechanism for defining a function in a header included by multiple translation units. Without `inline`, including `osal_port.h` from more than one `.cpp` file causes ODR link errors. `static` is an acceptable fallback (avoids ODR, but each TU gets its own copy of the function — functionally equivalent for a simple write call). The spec mandates `inline` in the template; `static` is documented as a toolchain workaround only.
- **Backend detection is static** (cmake `file(STRINGS...)`): computed once at cmake configure time, not at C++ compile time. This is intentional — changing backends requires re-running cmake. CMake does **not** validate that exactly one backend macro is defined; defining both `OSAL_BACKEND_POSIX` and `OSAL_BACKEND_CMSIS_OS` is undefined behaviour and the user's responsibility to avoid.
- **`osal_debug.cpp` is not changed**: it still calls `osal_port_debug_write`; the function is now resolved via the inline definition in `osal_port.h`.
- **No new RTOS backend is added**: this design enables third-party backends but does not implement them. Adding Zephyr, ThreadX, etc. is out of scope.
- **`OSAL_CONFIG_GOOGLETEST_ENABLE` opt-out is intentionally removed**: the new CMakeLists.txt always includes gtest sources. The existing opt-out mechanism (`OSAL_CONFIG_GOOGLETEST_ENABLE=0`) is removed as part of this redesign. This is a deliberate regression; builds that do not want gtest must exclude the gtest source files at their own CMake level.
