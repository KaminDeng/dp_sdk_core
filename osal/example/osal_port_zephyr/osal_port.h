// osal_port.h — Zephyr RTOS port using built-in CMSIS-RTOS v2 compatibility layer.
//
// Zephyr provides a native CMSIS-RTOS v2 implementation; no external adapter
// is needed. See:
// https://docs.zephyrproject.org/latest/services/portability/cmsis_rtos_v2.html
//
// Include path for cmsis_os2.h is exported by CMakeLists.txt via
// ${ZEPHYR_BASE}/include/zephyr/portability.
#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Section 1: Backend Selection
// ═══════════════════════════════════════════════════════════════════════════════
#define OSAL_BACKEND_CMSIS_OS

// ═══════════════════════════════════════════════════════════════════════════════
// Section 2: Platform Includes
//
// Zephyr's built-in CMSIS-RTOS v2 compatibility layer. The header is located at
// ${ZEPHYR_BASE}/include/zephyr/portability/cmsis_os2.h and is exported by the
// osal_port INTERFACE target in CMakeLists.txt.
//
// Zephyr does not implement all CMSIS-OS2 kernel management APIs. The stubs
// below suppress link/runtime errors for the subset used by osal's init path.
// osKernelInitialize and osKernelStart are no-ops because Zephyr's kernel is
// already running when application code executes. The remaining stubs abort
// with a printk message if accidentally called, making misuse visible.
// ═══════════════════════════════════════════════════════════════════════════════
#include <cmsis_os2.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// Zephyr kernel is already running at application entry — these are no-ops.
#define osKernelInitialize() ((void)0)
#define osKernelStart() ((void)0)

// These CMSIS-OS2 APIs have no Zephyr equivalent; abort loudly if called.
#define osKernelGetState()                                       \
    do {                                                         \
        printk("osKernelGetState is not supported on Zephyr\n"); \
        while (1)                                                \
            ;                                                    \
    } while (0)
#define osKernelSuspend()                                       \
    do {                                                        \
        printk("osKernelSuspend is not supported on Zephyr\n"); \
        while (1)                                               \
            ;                                                   \
    } while (0)
#define osKernelResume()                                       \
    do {                                                       \
        printk("osKernelResume is not supported on Zephyr\n"); \
        while (1)                                              \
            ;                                                  \
    } while (0)

// ═══════════════════════════════════════════════════════════════════════════════
// Section 3: Platform Constants
//
// OSAL_PORT_THREAD_MIN_STACK_SIZE — 512 bytes, consistent with Zephyr's
//   default CONFIG_MAIN_STACK_SIZE for minimal targets.
// OSAL_PORT_THREAD_DEFAULT_PRIORITY — osPriorityNormal from cmsis_os2.h.
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef OSAL_PORT_THREAD_MIN_STACK_SIZE
#define OSAL_PORT_THREAD_MIN_STACK_SIZE 512
#endif

#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#define OSAL_PORT_THREAD_DEFAULT_PRIORITY osPriorityNormal
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Section 4: Debug Output
//
// Uses Zephyr's printk(), which is safe to call from any thread context and
// does not require a file descriptor or POSIX runtime.
// ═══════════════════════════════════════════════════════════════════════════════
#include <cstdint>

inline void osal_port_debug_write(const char* buf, uint32_t len) { printk("%.*s", static_cast<int>(len), buf); }

// ═══════════════════════════════════════════════════════════════════════════════
// Section 5: Test Feature Flags
// ═══════════════════════════════════════════════════════════════════════════════
#define OSAL_TEST_ALL 1
