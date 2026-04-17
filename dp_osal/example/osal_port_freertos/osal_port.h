// osal/example/osal_port_freertos/osal_port.h
//
// STM32 FreeRTOS + CMSIS-OS2 port for the osal library.
// Targets STM32 MCUs running FreeRTOS via the CMSIS-OS2 wrapper (cmsis_os.h),
// with debug output over UART6 using the STM32 HAL.
//
// Usage in your project's CMakeLists.txt (before add_subdirectory(osal)):
//   set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/path/to/osal_port_freertos)
#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Section 1: Backend Selection
// Uncomment EXACTLY ONE line. Defining both is undefined behaviour.
// ═══════════════════════════════════════════════════════════════════════════════
// #define OSAL_BACKEND_POSIX
#define OSAL_BACKEND_CMSIS_OS

// ═══════════════════════════════════════════════════════════════════════════════
// Section 2: Platform Includes
//
// cmsis_os.h is the CMSIS-OS2 header supplied by the STM32 BSP / CubeMX.
// It wraps FreeRTOS APIs behind the standardised CMSIS-RTOS2 interface.
// ═══════════════════════════════════════════════════════════════════════════════
#include "cmsis_os.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Section 3: Platform Constants
//
// OSAL_PORT_THREAD_MIN_STACK_SIZE — minimum thread stack in bytes.
//   configMINIMAL_STACK_SIZE is defined in FreeRTOSConfig.h and gives the
//   smallest stack (in words) that the idle task uses; it is a safe lower
//   bound for any application thread.
//
// OSAL_PORT_THREAD_DEFAULT_PRIORITY — default thread priority.
//   osPriorityNormal maps to tskIDLE_PRIORITY + 1 in the CMSIS-OS2 wrapper,
//   which is the standard normal-priority level for FreeRTOS tasks.
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef OSAL_PORT_THREAD_MIN_STACK_SIZE
#define OSAL_PORT_THREAD_MIN_STACK_SIZE configMINIMAL_STACK_SIZE
#endif

#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#define OSAL_PORT_THREAD_DEFAULT_PRIORITY osPriorityNormal
#endif

// Optional: expose saved stack pointer from FreeRTOS TCB (pxTopOfStack).
// Keep this in the FreeRTOS port instead of OSAL core.
#ifndef OSAL_PORT_THREAD_STACK_POINTER_FROM_ID
#if defined(configUSE_TRACE_FACILITY) && (configUSE_TRACE_FACILITY == 1)
#define OSAL_PORT_THREAD_STACK_POINTER_FROM_ID(thread_id) (*reinterpret_cast<uint32_t *>(thread_id))
#else
#define OSAL_PORT_THREAD_STACK_POINTER_FROM_ID(thread_id) (0U)
#endif
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Section 4: Debug Output
//
// Implement osal_port_debug_write as an INLINE function.
//   buf: null-terminated log string (may contain embedded newlines)
//   len: number of bytes in buf (not including trailing NUL)
//
// The function is declared 'inline' because osal_port.h is included by
// multiple translation units. A plain (non-inline) definition in a header
// causes ODR link errors.
//
// This implementation sends output via HAL_UART_Transmit on UART6.
// Adjust the UART handle (huart6) and header (usart.h) to match your BSP.
// ═══════════════════════════════════════════════════════════════════════════════
#include <cstdint>

#include "usart.h"

inline void osal_port_debug_write(const char* buf, uint32_t len) {
    HAL_UART_Transmit(&huart6, reinterpret_cast<const uint8_t*>(buf), static_cast<uint16_t>(len), HAL_MAX_DELAY);
}

// NOTE: newlib stdout/stderr redirection (_write) cannot be defined inline
// because it requires 'extern "C"' linkage, which is incompatible with the
// 'inline' keyword in C++. If you need _write() redirection (e.g. so that
// printf/puts output goes to UART), add a separate .cpp file to your project
// with the following content:
//
//   #include <unistd.h>
//   #include "osal_port.h"
//
//   extern "C" int _write(int fd, const void* buf, size_t count) {
//       if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
//           osal_port_debug_write(static_cast<const char*>(buf),
//                                 static_cast<uint32_t>(count));
//           return static_cast<int>(count);
//       }
//       return -1;  // other file descriptors are not handled
//   }

// ═══════════════════════════════════════════════════════════════════════════════
// Section 5: Test Feature Flags
//
// OSAL_TEST_ALL = 1  → enable every test component (recommended default)
// OSAL_TEST_ALL = 0  → use the per-component flags below for fine control
//
// Per-component flags enable or skip entire test files (GTEST_SKIP).
// To disable a single test case within a component, use GTEST_SKIP() inside
// the test body with a runtime platform check rather than a compile-time macro.
//
// NOTE: The old osal_test_framework_config.h provided per-test-case control
// (e.g. TestOSALMemoryManagerGetAllocatedSizeEnabled, TestOSALRWLockGetReadLockCountEnabled,
// TestOSALRWLockIsWriteLockedEnabled were 0). The new format uses component-level
// flags only. Since all other tests within those components were enabled, all
// component flags are set to 1 here. Use GTEST_SKIP() at the test-case level
// inside the test body if specific cases need to be disabled at runtime.
//
// All components were enabled in the original configuration → OSAL_TEST_ALL = 1.
// ═══════════════════════════════════════════════════════════════════════════════
#define OSAL_TEST_ALL 1

#if !OSAL_TEST_ALL
// Set each to 1 (run tests) or 0 (GTEST_SKIP).
#define OSAL_TEST_THREAD_ENABLED 1              // all thread tests enabled
#define OSAL_TEST_CHRONO_ENABLED 1              // all chrono tests enabled
#define OSAL_TEST_MUTEX_ENABLED 1               // all mutex tests enabled
#define OSAL_TEST_CONDITION_VARIABLE_ENABLED 1  // all condition variable tests enabled
#define OSAL_TEST_LOCKGUARD_ENABLED 1           // all lockguard tests enabled
#define OSAL_TEST_MEMORY_MANAGER_ENABLED \
    1                                    // all memory manager tests enabled
                                         // (GetAllocatedSize was 0 in old format;
                                         //  per-case control now requires GTEST_SKIP)
#define OSAL_TEST_QUEUE_ENABLED 1        // all queue tests enabled
#define OSAL_TEST_SEMAPHORE_ENABLED 1    // all semaphore tests enabled
#define OSAL_TEST_THREAD_POOL_ENABLED 1  // all thread pool tests enabled
#define OSAL_TEST_TIMER_ENABLED 1        // all timer tests enabled
#define OSAL_TEST_RWLOCK_ENABLED \
    1                                 // all rwlock tests enabled
                                      // (GetReadLockCount=0, IsWriteLocked=0 in old
                                      //  format; per-case control now requires GTEST_SKIP)
#define OSAL_TEST_SPINLOCK_ENABLED 1  // all spinlock tests enabled
#endif
