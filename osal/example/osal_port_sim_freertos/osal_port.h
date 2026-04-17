// osal_port.h — FreeRTOS-on-POSIX simulator port using CMSIS-OS2 adapter.
//
// This port targets Linux/macOS POSIX simulators running FreeRTOS, using the
// cmsis_os2_freertos/ adapter in this directory to bridge CMSIS-OS2 APIs to
// FreeRTOS. Include paths for cmsis_os2_freertos/ are exported by CMakeLists.txt.
#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// Section 1: Backend Selection
// ═══════════════════════════════════════════════════════════════════════════════
#define OSAL_BACKEND_CMSIS_OS

// ═══════════════════════════════════════════════════════════════════════════════
// Section 2: Platform Includes
//
// cmsis_os2.h is provided by the cmsis_os2_freertos/ subdirectory in this
// directory. Its include path is exported via the osal_port INTERFACE target
// in CMakeLists.txt.
// ═══════════════════════════════════════════════════════════════════════════════
#include "cmsis_os2.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Section 3: Platform Constants
//
// configMINIMAL_STACK_SIZE is defined in FreeRTOSConfig.h (linked via
// freertos_config target).
// osPriorityNormal is from cmsis_os2.h above.
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
// Write directly to stdout fd (safe in POSIX simulator context).
// ═══════════════════════════════════════════════════════════════════════════════
#include <unistd.h>

#include <cstdint>
inline void osal_port_debug_write(const char* buf, uint32_t len) { ::write(1, buf, static_cast<size_t>(len)); }

// ═══════════════════════════════════════════════════════════════════════════════
// Section 5: Test Feature Flags
// ═══════════════════════════════════════════════════════════════════════════════
#define OSAL_TEST_ALL 1
