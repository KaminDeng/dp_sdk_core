# osal Single-File Port Interface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 4-file `osal_port` directory with a single `osal_port.h` file, with `OSAL_PORT_DIR` CMake variable for path independence and a built-in POSIX port for zero-config operation.

**Architecture:** New `port/builtin_posix/osal_port.h` and `port/template/osal_port.h` files are added to the osal library. `CMakeLists.txt` is rewritten to read `OSAL_PORT_DIR` and auto-detect the backend from `osal_port.h`. All 12 gtest files have their coarse-grained `TestOSALXxxEnabled` macros replaced with component-level `OSAL_TEST_YYY_ENABLED || OSAL_TEST_ALL` guards. The existing `applications/osal_port/` directory is migrated to the new single-file format.

**Tech Stack:** C++17, CMake 3.15+, Google Test, pthreads (POSIX backend)

**Working directory for all git commits:** `applications/osal/` (submodule)

**Build verification command (from project root):**
```bash
cmake -B build -S . -DCMAKE_OSX_SYSROOT:PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk && cmake --build build 2>&1 | tail -5
```

**Test run command:**
```bash
script -q /tmp/osal_port_out.txt ./build/posix_demo 2>/dev/null & sleep 15; kill %1 2>/dev/null; wait; grep -E "PASSED|FAILED|SKIPPED|tests ran" /tmp/osal_port_out.txt | tail -5
```

---

## File Map

| File | Action | Task |
|---|---|---|
| `port/builtin_posix/osal_port.h` | **Create** | Task 1 |
| `port/template/osal_port.h` | **Create** | Task 1 |
| `src/osal.h` | **Modify** | Task 2 |
| `src/impl/posix/include/osal_thread.h` | **Modify** | Task 2 |
| `CMakeLists.txt` | **Rewrite** | Task 3 |
| `test/osal_test_main.cpp` | **Modify** | Task 4 |
| `test/gtest/gtest_chrono.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_condition_variable.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_lockguard.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_memory_manger.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_mutex.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_queue.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_rwlock.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_semaphore.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_spin_lock.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_thread.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_thread_pool.cpp` | **Modify** | Task 5 |
| `test/gtest/gtest_timer.cpp` | **Modify** | Task 5 |
| `applications/osal_port/osal_port_config.h` | **Delete** | Task 6 |
| `applications/osal_port/osal_port_debug.cpp` | **Delete** | Task 6 |
| `applications/osal_port/osal_config.cmake` | **Delete** | Task 6 |
| `applications/osal_port/osal_test_framework_config.h` | **Delete** | Task 6 |
| `applications/osal_port/osal_port.h` | **Create** | Task 6 |
| `applications/osal_port/CMakeLists.txt` | **Modify** | Task 6 |

---

## Task 1: Create `port/` directory with built-in POSIX port and template

**Files:**
- Create: `port/builtin_posix/osal_port.h`
- Create: `port/template/osal_port.h`

- [ ] **Step 1: Create `port/builtin_posix/osal_port.h`**

Create the file at `applications/osal/port/builtin_posix/osal_port.h`:

```cpp
// osal/port/builtin_posix/osal_port.h
// Built-in POSIX port for the osal library.
// Used automatically when OSAL_PORT_DIR is not set by the consumer.
// Suitable for Linux, macOS, and any POSIX-compliant host environment.
//
// To use a custom port instead, set before add_subdirectory(osal):
//   set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)
#pragma once

// ── Section 1: Backend ────────────────────────────────────────────────────────
#define OSAL_BACKEND_POSIX

// ── Section 2: Platform Includes ─────────────────────────────────────────────
// (none required for POSIX)

// ── Section 3: Platform Constants ────────────────────────────────────────────
#include <climits>  // PTHREAD_STACK_MIN
#ifndef OSAL_PORT_THREAD_MIN_STACK_SIZE
#  ifdef PTHREAD_STACK_MIN
#    define OSAL_PORT_THREAD_MIN_STACK_SIZE PTHREAD_STACK_MIN
#  else
#    define OSAL_PORT_THREAD_MIN_STACK_SIZE 16384
#  endif
#endif

#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#  define OSAL_PORT_THREAD_DEFAULT_PRIORITY 0
#endif

// ── Section 4: Debug Output ───────────────────────────────────────────────────
#include <cstdint>
#include <unistd.h>

inline void osal_port_debug_write(const char* buf, uint32_t len) {
    ::write(1, buf, static_cast<size_t>(len));
}

// ── Section 5: Test Feature Flags ────────────────────────────────────────────
#ifndef OSAL_TEST_ALL
#  define OSAL_TEST_ALL 1
#endif
```

- [ ] **Step 2: Create `port/template/osal_port.h`**

Create the file at `applications/osal/port/template/osal_port.h`:

```cpp
// osal_port.h — Porting template for the osal library.
//
// HOW TO PORT osal TO A NEW PLATFORM:
//   1. Copy this file to your project's port directory (any name, any location).
//   2. Fill in each section below for your target platform.
//   3. In your project's CMakeLists.txt, BEFORE add_subdirectory(path/to/osal):
//        set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/your_port_dir)
//
// That's it. No other files are needed.
//
// BACKEND OPTIONS:
//   OSAL_BACKEND_POSIX    — std::thread, pthread, std::mutex, std::chrono
//                           Use for Linux, macOS, and POSIX-compatible hosts.
//   OSAL_BACKEND_CMSIS_OS — osThreadNew, osMutexNew, osTimerNew, etc.
//                           Use for FreeRTOS+CMSIS-OS2, Zephyr CMSIS-RTOS2, etc.
#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Section 1: Backend Selection
// Uncomment EXACTLY ONE line. Defining both is undefined behaviour.
// ═══════════════════════════════════════════════════════════════════════════════
#define OSAL_BACKEND_POSIX
// #define OSAL_BACKEND_CMSIS_OS

// ═══════════════════════════════════════════════════════════════════════════════
// Section 2: Platform Includes
//
// POSIX backend: leave empty — osal uses std::, pthread, and POSIX internally.
//
// CMSIS-OS backend: include the CMSIS-OS2 header for your RTOS. Examples:
//   FreeRTOS+CMSIS-OS2:  #include "cmsis_os2.h"  (from your BSP)
//   Zephyr:              #include <cmsis_os2.h>   (built-in Zephyr compatibility)
//   ThreadX CMSIS-OS2:   #include "tx_cmsis.h"
// ═══════════════════════════════════════════════════════════════════════════════
// #include "cmsis_os2.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Section 3: Platform Constants
//
// OSAL_PORT_THREAD_MIN_STACK_SIZE — minimum thread stack in bytes.
//   POSIX:     PTHREAD_STACK_MIN  (typically 16384 on Linux, 8192 on macOS)
//   FreeRTOS:  configMINIMAL_STACK_SIZE * sizeof(StackType_t)
//   Zephyr:    512 (or your CONFIG_MAIN_STACK_SIZE)
//
// OSAL_PORT_THREAD_DEFAULT_PRIORITY — default thread priority.
//   POSIX:    0   (not meaningfully used; POSIX priority requires root)
//   CMSIS-OS: osPriorityNormal
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef OSAL_PORT_THREAD_MIN_STACK_SIZE
#  define OSAL_PORT_THREAD_MIN_STACK_SIZE  4096
#endif

#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#  define OSAL_PORT_THREAD_DEFAULT_PRIORITY  0
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Section 4: Debug Output
//
// Implement osal_port_debug_write as an INLINE function.
//   buf: null-terminated log string (may contain embedded newlines)
//   len: number of bytes in buf (not including trailing NUL)
//
// The function must be declared 'inline' because osal_port.h is included
// by multiple translation units. A plain (non-inline, non-static) definition
// in a header causes ODR link errors. 'static' is also acceptable if your
// toolchain has issues with 'inline', but produces a private copy per TU.
//
// Must be safe to call from task/thread context.
//
// Examples — uncomment and adapt ONE implementation:
// ═══════════════════════════════════════════════════════════════════════════════
#include <cstdint>

// POSIX / Linux / macOS — write to stdout:
#include <unistd.h>
inline void osal_port_debug_write(const char* buf, uint32_t len) {
    ::write(1, buf, static_cast<size_t>(len));
}

// STM32 HAL UART (replace with your UART handle and header):
// #include "usart.h"
// inline void osal_port_debug_write(const char* buf, uint32_t len) {
//     HAL_UART_Transmit(&huart6, reinterpret_cast<const uint8_t*>(buf),
//                       static_cast<uint16_t>(len), HAL_MAX_DELAY);
// }

// Zephyr printk:
// #include <zephyr/sys/printk.h>
// inline void osal_port_debug_write(const char* buf, uint32_t len) {
//     printk("%.*s", static_cast<int>(len), buf);
// }

// ═══════════════════════════════════════════════════════════════════════════════
// Section 5: Test Feature Flags
//
// OSAL_TEST_ALL = 1  → enable every test component (recommended for first bring-up)
// OSAL_TEST_ALL = 0  → use the per-component flags below for fine control
//
// Per-component flags enable or skip entire test files (GTEST_SKIP).
// To disable a single test case within a component, use GTEST_SKIP() inside
// the test body with a runtime platform check rather than a compile-time macro.
// ═══════════════════════════════════════════════════════════════════════════════
#define OSAL_TEST_ALL 1

#if !OSAL_TEST_ALL
// Set each to 1 (run tests) or 0 (GTEST_SKIP).
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

- [ ] **Step 3: Verify files exist**

```bash
ls applications/osal/port/builtin_posix/osal_port.h applications/osal/port/template/osal_port.h
```

Expected: both files listed.

- [ ] **Step 4: Commit**

In `applications/osal/` submodule:
```bash
git add port/builtin_posix/osal_port.h port/template/osal_port.h
git commit -m "feat: add port/ directory with built-in POSIX port and user template

- port/builtin_posix/osal_port.h: zero-config POSIX port (stdout debug,
  PTHREAD_STACK_MIN, OSAL_TEST_ALL=1); used when OSAL_PORT_DIR is not set
- port/template/osal_port.h: fully documented porting template covering
  all 5 sections (backend, includes, constants, debug output, test flags)"
```

---

## Task 2: Update `src/osal.h` and fix POSIX include chain

**Files:**
- Modify: `src/osal.h`
- Modify: `src/impl/posix/include/osal_thread.h` (add `#include "osal.h"` and rename macros)
- Modify: `src/impl/cmsis_os/include/osal_thread.h` (rename macros only)

- [ ] **Step 1: Rewrite `src/osal.h`**

Replace the entire content of `applications/osal/src/osal.h` with:

```cpp
// osal.h — Entry point for the osal library's platform configuration.
//
// This header is included by all osal implementation files to pull in
// osal_port.h (the single-file user port). It must resolve via the
// include path set by CMakeLists.txt from OSAL_PORT_DIR.
//
// Users: do NOT include this file directly. Include the individual
// component headers (osal_thread.h, osal_mutex.h, etc.) instead.
#pragma once

#if __has_include("osal_port.h")
#  include "osal_port.h"
#else
#  error "[osal] osal_port.h not found. " \
         "Set OSAL_PORT_DIR in your CMakeLists.txt BEFORE add_subdirectory(osal):\n" \
         "  set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/your_port_dir)\n" \
         "Template: copy osal/port/template/osal_port.h to your port directory."
#endif
```

- [ ] **Step 2: Add `#include "osal.h"` to POSIX `osal_thread.h` and rename macros**

In `applications/osal/src/impl/posix/include/osal_thread.h`:

**Add** `#include "osal.h"` after `#include "osal_debug.h"` (currently line 17):

```cpp
#include "interface_thread.h"
#include "osal.h"        // ← ADD this line (provides OSAL_PORT_THREAD_MIN_STACK_SIZE)
#include "osal_debug.h"
```

**Replace** both occurrences of `OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE` (lines 48–49):
```cpp
// Old:
int size = (stack_size > OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE) ? stack_size
                                                                : OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE;
// New:
int size = (stack_size > OSAL_PORT_THREAD_MIN_STACK_SIZE) ? stack_size
                                                           : OSAL_PORT_THREAD_MIN_STACK_SIZE;
```

- [ ] **Step 3: Rename macros in CMSIS-OS `osal_thread.h`**

In `applications/osal/src/impl/cmsis_os/include/osal_thread.h`, lines 39–41:
```cpp
// Old:
attr.stack_size = (stack_size > OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE)
                      ? stack_size
                      : OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE;
// New:
attr.stack_size = (stack_size > OSAL_PORT_THREAD_MIN_STACK_SIZE)
                      ? stack_size
                      : OSAL_PORT_THREAD_MIN_STACK_SIZE;
```

Also check the same file for any use of `OSAL_CONFIG_THREAD_DEFAULT_PRIORITY` and rename to `OSAL_PORT_THREAD_DEFAULT_PRIORITY`. Search: `grep -n "OSAL_CONFIG_THREAD" applications/osal/src/impl/cmsis_os/include/osal_thread.h`

- [ ] **Step 4: Search for remaining old macro names across all impl files**

```bash
grep -rn "OSAL_CONFIG_THREAD" applications/osal/src/
```

Fix any remaining occurrences found (rename `OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE` → `OSAL_PORT_THREAD_MIN_STACK_SIZE` and `OSAL_CONFIG_THREAD_DEFAULT_PRIORITY` → `OSAL_PORT_THREAD_DEFAULT_PRIORITY`).

- [ ] **Step 5: Commit**

```bash
git add src/osal.h src/impl/posix/include/osal_thread.h src/impl/cmsis_os/include/osal_thread.h
git commit -m "feat: osal.h includes osal_port.h; rename port constants (Task 2)

- src/osal.h: now includes osal_port.h (was osal_port_config.h);
  improved error message guides user to set OSAL_PORT_DIR
- posix/osal_thread.h: add #include \"osal.h\" to resolve
  OSAL_PORT_THREAD_MIN_STACK_SIZE in the POSIX include chain
- cmsis_os/osal_thread.h: rename OSAL_CONFIG_THREAD_MINIMAL_STACK_SIZE
  → OSAL_PORT_THREAD_MIN_STACK_SIZE (osal.h already included)"
```

---

## Task 3: Rewrite `CMakeLists.txt`

**Files:**
- Modify: `CMakeLists.txt` (osal submodule root, currently 60 lines)

This is the most critical task. The new CMakeLists.txt removes the hard-coded
`../osal_port/` path, reads `OSAL_PORT_DIR`, auto-detects backend from `osal_port.h`,
and always links gtest.

- [ ] **Step 1: Replace CMakeLists.txt with new content**

Replace the entire content of `applications/osal/CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── Port directory resolution ──────────────────────────────────────────────────
# Users set OSAL_PORT_DIR before add_subdirectory(osal) to supply their port:
#   set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)
#
# If OSAL_PORT_DIR is not set, the built-in POSIX port is used (stdout debug,
# all tests enabled). This is suitable for Linux/macOS development out of the box.
if (NOT DEFINED OSAL_PORT_DIR)
    message(STATUS "[osal] OSAL_PORT_DIR not set — using built-in POSIX port (port/builtin_posix)")
    set(OSAL_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port/builtin_posix")
endif()

if (NOT EXISTS "${OSAL_PORT_DIR}/osal_port.h")
    message(FATAL_ERROR
        "[osal] osal_port.h not found in OSAL_PORT_DIR='${OSAL_PORT_DIR}'.\n"
        "Copy osal/port/template/osal_port.h to '${OSAL_PORT_DIR}/' and fill it in.\n"
        "Then set OSAL_PORT_DIR to that directory before add_subdirectory(osal).")
endif()

# ── Backend auto-detection ─────────────────────────────────────────────────────
# Scan osal_port.h for the OSAL_BACKEND_CMSIS_OS macro (non-commented line).
# Defaults to posix if not found. Defining both backends is undefined behaviour.
file(STRINGS "${OSAL_PORT_DIR}/osal_port.h" _osal_port_lines)
set(_osal_backend "posix")
foreach(_line IN LISTS _osal_port_lines)
    if (_line MATCHES "^[[:space:]]*#[[:space:]]*define[[:space:]]+OSAL_BACKEND_CMSIS_OS")
        set(_osal_backend "cmsis_os")
        break()
    endif()
endforeach()
message(STATUS "[osal] Port directory: ${OSAL_PORT_DIR}")
message(STATUS "[osal] Backend:        ${_osal_backend}")

# ── Source selection ───────────────────────────────────────────────────────────
set(OSAL_IMPL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/impl")
set(GOOGLETEST_SUITE_PATH "test/gtest")

file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${OSAL_IMPL_PATH}/${_osal_backend}/*.cpp")
file(GLOB_RECURSE GTEST_SUITE_SOURCES "${GOOGLETEST_SUITE_PATH}/*.cpp")

message(STATUS "[osal] Impl sources:   ${SYSTEM_IMPL_SOURCES}")

# ── Library target ─────────────────────────────────────────────────────────────
add_library(osal STATIC
    src/debug/osal_debug.cpp
    ${SYSTEM_IMPL_SOURCES}
    test/osal_test_main.cpp
    ${GTEST_SUITE_SOURCES}
)

# ── Include directories ────────────────────────────────────────────────────────
# OSAL_PORT_DIR is PUBLIC so downstream targets can also see osal_port.h
# (needed if they use osal types directly or include any osal header).
target_include_directories(osal
    PUBLIC
        "${OSAL_PORT_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/debug"
        $<$<STREQUAL:${_osal_backend},cmsis_os>:${CMAKE_CURRENT_SOURCE_DIR}/src/impl/cmsis_os/include>
        $<$<STREQUAL:${_osal_backend},posix>:${CMAKE_CURRENT_SOURCE_DIR}/src/impl/posix/include>
        "${CMAKE_CURRENT_SOURCE_DIR}/test"
        "${CMAKE_CURRENT_SOURCE_DIR}/${GOOGLETEST_SUITE_PATH}"
)

# ── Dependencies ───────────────────────────────────────────────────────────────
# gtest is always linked (test sources always compiled).
# For CMSIS-OS ports that depend on additional platform libraries, the user
# must link them externally:
#   target_link_libraries(osal PUBLIC freertos_kernel cmsis_os2_impl)
target_link_libraries(osal PUBLIC gtest)
```

- [ ] **Step 2: Verify CMake configures successfully with the built-in port**

> **Important:** Do NOT run full cmake configure yet — `gtest` ordering will be fixed in
> Task 6. Instead, just verify the osal `CMakeLists.txt` is syntactically valid:

```bash
cmake --no-warn-unused-cli -P applications/osal/CMakeLists.txt 2>&1 | head -5
```

If that doesn't work cleanly, just proceed to Task 6 which fixes the ordering issue, then
do a full build verification in Task 6 Step 6.

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "feat: rewrite CMakeLists.txt — OSAL_PORT_DIR + auto backend detection (Task 3)

- Remove hard-coded ../osal_port/osal_config.cmake include
- Add OSAL_PORT_DIR variable; falls back to port/builtin_posix when unset
- Detect backend (posix/cmsis_os) by scanning osal_port.h for
  OSAL_BACKEND_CMSIS_OS (non-commented define)
- Always compile gtest sources; link gtest unconditionally
- Use absolute paths in target_include_directories for clarity
- Drop OSAL_CONFIG_GOOGLETEST_ENABLE opt-out (intentional regression)"
```

---

## Task 4: Update `test/osal_test_main.cpp`

**Files:**
- Modify: `test/osal_test_main.cpp`

Remove the `OSAL_CONFIG_GOOGLETEST_ENABLE` guards since gtest is now always active.

- [ ] **Step 1: Replace the file content**

Replace `applications/osal/test/osal_test_main.cpp` with:

```cpp
//
// osal_test_main.cpp — Test entry point for the osal library.
//
// All gtest test files are #included here so they compile as part of
// the osal static library (no separate test executable is needed;
// osal_test_main() is called from the host application's task).
//
// Test feature flags (OSAL_TEST_ALL, OSAL_TEST_THREAD_ENABLED, etc.)
// are defined in osal_port.h (Section 5).
//
#include "osal_debug.h"
#include "osal_system.h"
#include "osal_thread.h"

using namespace osal;

#include <gtest/gtest.h>

// Include all gtest test files. Each file guards its tests with
// #if (OSAL_TEST_YYY_ENABLED || OSAL_TEST_ALL)
#include "gtest_chrono.cpp"
#include "gtest_condition_variable.cpp"
#include "gtest_lockguard.cpp"
#include "gtest_memory_manger.cpp"   // note: filename typo is intentional
#include "gtest_mutex.cpp"
#include "gtest_queue.cpp"
#include "gtest_rwlock.cpp"
#include "gtest_semaphore.cpp"
#include "gtest_spin_lock.cpp"
#include "gtest_thread.cpp"
#include "gtest_thread_pool.cpp"
#include "gtest_timer.cpp"

void StartDefaultTask(void *argument) {
    (void)argument;
    // Uncomment to enable verbose osal logging during tests:
    // setLogLevel(LOG_LEVEL_VERBOSE);

    int argc = 1;
    char *argv[1] = {const_cast<char *>("osal_gtest")};
    ::testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
}

OSALThread osal_test_thread;

extern "C" int osal_test_main(void) {
    static int arg = 12;
    OSAL_LOGI("System Type: %s\n", OSALSystem::getInstance().get_system_info());
    osal_test_thread.start("osal_test_thread", StartDefaultTask,
                           static_cast<void *>(&arg), 0, 2048);
    OSALSystem::getInstance().StartScheduler();
    return 0;
}
```

- [ ] **Step 2: Commit**

```bash
git add test/osal_test_main.cpp
git commit -m "feat: remove OSAL_CONFIG_GOOGLETEST_ENABLE guards from osal_test_main.cpp (Task 4)

gtest is now always active. Remove conditional compilation wrappers.
Test feature flags are now in osal_port.h Section 5 (OSAL_TEST_ALL etc.)."
```

---

## Task 5: Update all 12 gtest files — replace test-enable macros

**Files:** All 12 `test/gtest/gtest_*.cpp` files

For each file, the `#include "osal_test_framework_config.h"` line is removed, and
every `#if (TestOSALXxxEnabled)` guard is replaced with the corresponding component flag.

**Component → macro mapping:**
| gtest file | Old include guard pattern | New guard |
|---|---|---|
| `gtest_chrono.cpp` | `TestOSALChronoXxxEnabled` | `OSAL_TEST_CHRONO_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_condition_variable.cpp` | `TestOSALConditionVariableXxxEnabled` | `OSAL_TEST_CONDITION_VARIABLE_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_lockguard.cpp` | `TestOSALLockGuardXxxEnabled` | `OSAL_TEST_LOCKGUARD_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_memory_manger.cpp` | `TestOSALMemoryManagerXxxEnabled` | `OSAL_TEST_MEMORY_MANAGER_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_mutex.cpp` | `TestOSALMutexXxxEnabled` | `OSAL_TEST_MUTEX_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_queue.cpp` | `TestOSALMessageQueueXxxEnabled` | `OSAL_TEST_QUEUE_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_rwlock.cpp` | `TestOSALRWLockXxxEnabled` | `OSAL_TEST_RWLOCK_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_semaphore.cpp` | `TestOSALSemaphoreXxxEnabled` | `OSAL_TEST_SEMAPHORE_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_spin_lock.cpp` | `TestOSALSpinLockXxxEnabled` | `OSAL_TEST_SPINLOCK_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_thread.cpp` | `TestOSALThreadXxxEnabled` | `OSAL_TEST_THREAD_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_thread_pool.cpp` | `TestOSALThreadPoolXxxEnabled` | `OSAL_TEST_THREAD_POOL_ENABLED \|\| OSAL_TEST_ALL` |
| `gtest_timer.cpp` | `TestOSALTimerXxxEnabled` | `OSAL_TEST_TIMER_ENABLED \|\| OSAL_TEST_ALL` |

- [ ] **Step 1: Process each gtest file — remove old include, replace macro guards**

For each file:

1. Remove the line: `#include "osal_test_framework_config.h"`
2. Replace every `#if (TestOSALXxxEnabled)` with `#if (OSAL_TEST_YYY_ENABLED || OSAL_TEST_ALL)`
   using the mapping table above. The `Xxx` part varies per test (e.g. `TestOSALThreadStartEnabled`,
   `TestOSALThreadStopEnabled`, etc.) but they ALL map to the same component flag per file.
3. `#else` and `#endif` lines are unchanged.

Example — `gtest_thread.cpp` before/after for the first test:

Before:
```cpp
#include "osal_test_framework_config.h"
...
TEST(OSALThreadTest, TestOSALThreadStart) {
#if (TestOSALThreadStartEnabled)
    ...
#else
    GTEST_SKIP();
#endif
}
```

After:
```cpp
// (no osal_test_framework_config.h include)
...
TEST(OSALThreadTest, TestOSALThreadStart) {
#if (OSAL_TEST_THREAD_ENABLED || OSAL_TEST_ALL)
    ...
#else
    GTEST_SKIP();
#endif
}
```

Apply this pattern to every test in every file. Each file has between 4 and 10 tests;
each gets its `#if` guard updated to the same component-level macro.

- [ ] **Step 2: Verify no old macro references remain**

```bash
grep -rn "TestOSAL\|osal_test_framework_config" applications/osal/test/gtest/
```

Expected: no output.

- [ ] **Step 3: Commit**

```bash
git add test/gtest/
git commit -m "feat: replace per-test macros with component-level OSAL_TEST_* flags (Task 5)

Remove #include \"osal_test_framework_config.h\" from all 12 gtest files.
Replace 60+ per-test TestOSALXxxEnabled macros with 12 component-level flags:
  OSAL_TEST_THREAD_ENABLED, OSAL_TEST_MUTEX_ENABLED, ... || OSAL_TEST_ALL
All tests enabled by default via OSAL_TEST_ALL=1 in osal_port.h."
```

---

## Task 6: Migrate `applications/osal_port/` to new single-file format

**Files (in `applications/osal_port/`, NOT inside the osal submodule):**
- Create: `applications/osal_port/osal_port.h`
- Delete: `applications/osal_port/osal_port_config.h`
- Delete: `applications/osal_port/osal_port_debug.cpp`
- Delete: `applications/osal_port/osal_config.cmake`
- Delete: `applications/osal_port/osal_test_framework_config.h`
- Modify: `applications/osal_port/CMakeLists.txt`
- Modify: `CMakeLists.txt` (top-level project root) — set `OSAL_PORT_DIR`

This task works in the **parent repo** (project root), not the osal submodule.

- [ ] **Step 1: Create `applications/osal_port/osal_port.h`**

Create `applications/osal_port/osal_port.h` with the following content, which is the
functional equivalent of the four files it replaces:

```cpp
// applications/osal_port/osal_port.h
// POSIX simulation port for the osal library.
// This is the port used by the freertos_dpcpp project's posix_demo binary.
#pragma once

// ── Section 1: Backend ────────────────────────────────────────────────────────
#define OSAL_BACKEND_POSIX

// ── Section 2: Platform Includes ─────────────────────────────────────────────
// (none required for POSIX)

// ── Section 3: Platform Constants ────────────────────────────────────────────
#include <climits>
#ifndef OSAL_PORT_THREAD_MIN_STACK_SIZE
#  ifdef PTHREAD_STACK_MIN
#    define OSAL_PORT_THREAD_MIN_STACK_SIZE PTHREAD_STACK_MIN
#  else
#    define OSAL_PORT_THREAD_MIN_STACK_SIZE 16384
#  endif
#endif

#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#  define OSAL_PORT_THREAD_DEFAULT_PRIORITY 0
#endif

// ── Section 4: Debug Output ───────────────────────────────────────────────────
#include <cstdint>
#include <unistd.h>

inline void osal_port_debug_write(const char* buf, uint32_t len) {
    ::write(1, buf, static_cast<size_t>(len));
}

// ── Section 5: Test Feature Flags ────────────────────────────────────────────
// Enable all tests. Individual components can be disabled by setting
// OSAL_TEST_ALL to 0 and defining the per-component flags.
#define OSAL_TEST_ALL 1
```

- [ ] **Step 2: Delete the four obsolete files**

From the project root (parent repo):
```bash
git rm applications/osal_port/osal_port_config.h
git rm applications/osal_port/osal_port_debug.cpp
git rm applications/osal_port/osal_config.cmake
git rm applications/osal_port/osal_test_framework_config.h
```

- [ ] **Step 3: Update `applications/osal_port/CMakeLists.txt`**

The current `osal_port` is a STATIC library that:
1. Compiles `osal_port_debug.cpp` (and any other `.cpp` in the directory)
2. Exports include paths for `osal_port_config.h`, `cmsis_os2_freertos/`, etc.
3. Links `freertos_kernel` and `freertos_config` PUBLIC (providing transitive deps to consumers)

After this change, `osal_port_debug.cpp` is gone (debug output is inline in `osal_port.h`).
The library must become INTERFACE — **but must retain the `freertos_kernel freertos_config`
transitive link**, because `posix_demo` currently receives those dependencies through
`osal_port`. Removing them without replacing the transitive link would break the
`posix_demo` link step.

Replace `applications/osal_port/CMakeLists.txt` with:

```cmake
# applications/osal_port/CMakeLists.txt
#
# osal_port is now a header-only port directory.
# osal_port_debug_write is defined inline in osal_port.h — no .cpp sources.
#
# The INTERFACE library retains the freertos_kernel / freertos_config PUBLIC link
# so that consumers (posix_demo) continue to receive those transitive dependencies
# through the osal_port target, which is listed in USER_DEPEND_LIB in the
# top-level CMakeLists.txt.
add_library(osal_port INTERFACE)

target_include_directories(osal_port INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmsis_os2_freertos/cmsis_base
    ${CMAKE_CURRENT_SOURCE_DIR}/cmsis_os2_freertos
)

# Keep transitive FreeRTOS dependencies so posix_demo's link is unchanged.
target_link_libraries(osal_port INTERFACE freertos_kernel freertos_config)
```

- [ ] **Step 4: Update `applications/CMakeLists.txt` — reorder to put googletest before osal**

The current order is:
```cmake
add_subdirectory(osal_port)
add_subdirectory(osal)          # osal links to gtest — but gtest not yet defined!
add_subdirectory(googletest)    # gtest defined here
add_subdirectory(benchmark)
```

The new `osal/CMakeLists.txt` (Task 3) does `target_link_libraries(osal PUBLIC gtest)`.
CMake requires `gtest` to be defined before this call. Reorder to:

```cmake
cmake_minimum_required(VERSION 3.15)

add_subdirectory(osal_port)
add_subdirectory(googletest)   # ← moved up: gtest must exist before osal links to it
add_subdirectory(osal)
add_subdirectory(benchmark)
```

- [ ] **Step 5: Update top-level `CMakeLists.txt` — set `OSAL_PORT_DIR` and remove old config references**

In the top-level `CMakeLists.txt` (project root), find the block around line 87-97
(`add_subdirectory` calls). Add `OSAL_PORT_DIR` before `add_subdirectory(applications)`:

```cmake
# ── osal port directory ────────────────────────────────────────────────────────
# Point the osal library at the project's port implementation.
# Must appear BEFORE add_subdirectory(applications).
set(OSAL_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/applications/osal_port")
```

Then search for and remove any remaining references to the old cmake variables:
```bash
grep -n "OSAL_CONFIG_PLATFORM\|OSAL_CONFIG_DEPENDENT_LIBRARIES\|OSAL_CONFIG_GOOGLETEST_ENABLE\|OSAL_CONFIG_SELFTEST" \
  /path/to/project/root/CMakeLists.txt
```

Remove any such lines found (they were provided by the now-deleted `osal_config.cmake`).

Also, the top-level file previously relied on `OSAL_CONFIG_DEPENDENT_LIBRARIES`
(`osal_port gtest`) being set by `osal_config.cmake`. Since that file is now deleted,
search the top-level `CMakeLists.txt` for any remaining references to
`OSAL_CONFIG_DEPENDENT_LIBRARIES`, `OSAL_CONFIG_PLATFORM`, or `OSAL_CONFIG_GOOGLETEST_ENABLE`
and remove them.

```bash
grep -n "OSAL_CONFIG" /path/to/CMakeLists.txt  # adjust path
```

- [ ] **Step 5: Full build verification**

```bash
cmake -B build -S . -DCMAKE_OSX_SYSROOT:PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk 2>&1 | grep -E "\[osal\]|error:|FATAL"
cmake --build build 2>&1 | tail -10
```

Expected:
```
[osal] Port directory: .../applications/osal_port
[osal] Backend:        posix
...
[100%] Built target posix_demo
```

No errors.

- [ ] **Step 6: Run tests**

```bash
script -q /tmp/osal_port_test.txt ./build/posix_demo 2>/dev/null & sleep 20; kill %1 2>/dev/null; wait; grep -E "PASSED|FAILED|SKIPPED|tests ran" /tmp/osal_port_test.txt | tail -5
```

Expected: same pass/skip count as before this change (67 PASSED, 3 SKIPPED, 0 FAILED).

- [ ] **Step 7: Commit (parent repo)**

From the project root:
```bash
git add applications/osal_port/osal_port.h
git add applications/osal_port/CMakeLists.txt
git add applications/CMakeLists.txt    # googletest reorder
git add CMakeLists.txt                 # OSAL_PORT_DIR setting
git add applications/osal              # submodule pointer update
git commit -m "feat: migrate osal_port/ to single-file osal_port.h; set OSAL_PORT_DIR

- applications/osal_port/osal_port.h: new single-file POSIX port replacing
  osal_port_config.h, osal_port_debug.cpp, osal_config.cmake, and
  osal_test_framework_config.h; osal_port_debug_write is now inline
- applications/osal_port/CMakeLists.txt: INTERFACE library (no .cpp sources);
  retains freertos_kernel/freertos_config link for posix_demo transitivity
- applications/CMakeLists.txt: move googletest before osal so gtest target
  exists when osal's CMakeLists.txt links against it
- CMakeLists.txt: set OSAL_PORT_DIR before add_subdirectory(applications)
- Removed: osal_port_config.h, osal_port_debug.cpp, osal_config.cmake,
  osal_test_framework_config.h"
```

---

## Task 7: Final verification and osal submodule commit

- [ ] **Step 1: Verify no references to old files remain**

```bash
grep -rn "osal_port_config\|osal_config\.cmake\|osal_test_framework_config\|OSAL_CONFIG_PLATFORM\|OSAL_CONFIG_DEPENDENT_LIBRARIES\|OSAL_CONFIG_GOOGLETEST_ENABLE\|TestOSALChronoNowEnabled\|TestOSAL" \
  applications/osal/ applications/osal_port/ CMakeLists.txt 2>/dev/null | grep -v ".git"
```

Expected: no output (or only matches in `docs/` and `example/` directories which are informational and not compiled).

- [ ] **Step 2: Verify the `port/` examples still work as documentation**

The `applications/osal/example/` directories reference `osal_config.cmake` in their
own copies (they are standalone example projects, not part of the build). They are
**not updated** in this plan — they serve as historical reference and are excluded from
the build. Confirm they are not part of the cmake target:

```bash
grep -rn "example" applications/osal/CMakeLists.txt
```

Expected: no output (examples are not add_subdirectory'd).

- [ ] **Step 3: Clean build + full test run**

```bash
rm -rf build && cmake -B build -S . -DCMAKE_OSX_SYSROOT:PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk 2>&1 | grep -E "\[osal\]|FATAL|error:" && cmake --build build 2>&1 | tail -5
```

Expected: osal status messages print correctly, build succeeds.

```bash
script -q /tmp/osal_final.txt ./build/posix_demo 2>/dev/null & sleep 20; kill %1 2>/dev/null; wait; grep -E "PASSED|FAILED|SKIPPED|tests ran" /tmp/osal_final.txt | tail -5
```

Expected: 67 PASSED, 3 SKIPPED, 0 FAILED.

- [ ] **Step 4: Commit osal submodule final state**

Inside `applications/osal/`:
```bash
git log --oneline -5  # confirm all Task 1-5 commits are present
```

If any uncommitted changes remain in the submodule, commit them now.

- [ ] **Step 5: Update parent repo submodule pointer**

From project root:
```bash
git add applications/osal
git status  # confirm submodule pointer updated
```

---

## Verification Checklist

After all tasks complete, confirm:

- [ ] `osal/port/builtin_posix/osal_port.h` exists and compiles with POSIX backend
- [ ] `osal/port/template/osal_port.h` exists with all 5 sections documented
- [ ] `osal/CMakeLists.txt` reads `OSAL_PORT_DIR` (no hard-coded `../osal_port/` path)
- [ ] `osal/src/osal.h` includes `osal_port.h` (not `osal_port_config.h`)
- [ ] `posix/osal_thread.h` has `#include "osal.h"` and uses `OSAL_PORT_THREAD_MIN_STACK_SIZE`
- [ ] `cmsis_os/osal_thread.h` uses `OSAL_PORT_THREAD_MIN_STACK_SIZE`
- [ ] Zero gtest files contain `osal_test_framework_config` or `TestOSALXxx` macros
- [ ] `osal_test_main.cpp` has no `OSAL_CONFIG_GOOGLETEST_ENABLE` guards
- [ ] `applications/osal_port/osal_port.h` exists
- [ ] `applications/osal_port/osal_port_config.h`, `osal_port_debug.cpp`, `osal_config.cmake`, `osal_test_framework_config.h` are deleted
- [ ] `applications/osal_port/CMakeLists.txt` is an INTERFACE library
- [ ] Top-level `CMakeLists.txt` sets `OSAL_PORT_DIR` before `add_subdirectory(applications)`
- [ ] Clean build succeeds with no errors
- [ ] Tests: 67 PASSED, 3 SKIPPED, 0 FAILED
