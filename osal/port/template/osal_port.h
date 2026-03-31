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
#define OSAL_PORT_THREAD_MIN_STACK_SIZE 4096
#endif

#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#define OSAL_PORT_THREAD_DEFAULT_PRIORITY 0
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
inline void osal_port_debug_write(const char* buf, uint32_t len) { ::write(1, buf, static_cast<size_t>(len)); }

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
#define OSAL_TEST_THREAD_ENABLED 1
#define OSAL_TEST_CHRONO_ENABLED 1
#define OSAL_TEST_MUTEX_ENABLED 1
#define OSAL_TEST_CONDITION_VARIABLE_ENABLED 1
#define OSAL_TEST_LOCKGUARD_ENABLED 1
#define OSAL_TEST_MEMORY_MANAGER_ENABLED 1
#define OSAL_TEST_QUEUE_ENABLED 1
#define OSAL_TEST_SEMAPHORE_ENABLED 1
#define OSAL_TEST_SPINLOCK_ENABLED 1
#define OSAL_TEST_RWLOCK_ENABLED 1
#define OSAL_TEST_TIMER_ENABLED 1
#define OSAL_TEST_THREAD_POOL_ENABLED 1
#endif
