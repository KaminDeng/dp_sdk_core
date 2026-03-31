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
#ifdef PTHREAD_STACK_MIN
#define OSAL_PORT_THREAD_MIN_STACK_SIZE PTHREAD_STACK_MIN
#else
#define OSAL_PORT_THREAD_MIN_STACK_SIZE 16384
#endif
#endif

#ifndef OSAL_PORT_THREAD_DEFAULT_PRIORITY
#define OSAL_PORT_THREAD_DEFAULT_PRIORITY 0
#endif

// ── Section 4: Debug Output ───────────────────────────────────────────────────
#include <unistd.h>

#include <cstdint>

inline void osal_port_debug_write(const char* buf, uint32_t len) { ::write(1, buf, static_cast<size_t>(len)); }

// ── Section 5: Test Feature Flags ────────────────────────────────────────────
#ifndef OSAL_TEST_ALL
#define OSAL_TEST_ALL 1
#endif
