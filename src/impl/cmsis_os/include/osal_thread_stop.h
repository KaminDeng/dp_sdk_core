// osal_thread_stop.h — Cooperative thread-stop support for CMSIS-OS backend.
//
// Provides the stop exception class and per-thread stop-flag access.
//
// Two backends:
//   POSIX/desktop (default): C++ inline thread_local pointer — zero overhead.
//   ARM bare-metal (DPSDK_BARE_METAL): FreeRTOS per-task TLS slot 0.
//     Requires configNUM_THREAD_LOCAL_STORAGE_POINTERS >= 1 in FreeRTOSConfig.h.
#pragma once

#include <atomic>
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#include <exception>
#endif

namespace osal {

/* Thrown by OSALSystem::sleep_ms() when the current thread's stop flag is set.
 * Caught by OSALThread::taskRunner() so code after the sleep call is skipped.
 * Under -fno-exceptions (bare-metal), the struct has no base class and is never
 * thrown; stop() relies on cooperative task exit instead. */
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
struct OSALCmsisThreadStopException : public std::exception {
    const char *what() const noexcept override { return "OSALThread (CMSIS-OS): stop requested"; }
};
#else
struct OSALCmsisThreadStopException {};
#endif

/* ── Per-thread stop-flag access ──────────────────────────────────────────── */

#ifdef DPSDK_BARE_METAL
/* ARM bare-metal: use FreeRTOS per-task TLS slot 0.
 * Thread-local pointer to the running OSALThread's stop flag.
 * Set by taskRunner() before calling the user function; cleared on exit.
 * sleep_ms() reads this to detect stop requests.                            */
#include "FreeRTOS.h"
#include "task.h"

/** @brief Get the current task's stop flag pointer from FreeRTOS TLS slot 0. */
inline std::atomic<bool>* osal_cmsis_stop_flag_get() {
    return static_cast<std::atomic<bool>*>(pvTaskGetThreadLocalStoragePointer(nullptr, 0));
}

/** @brief Set the current task's stop flag pointer in FreeRTOS TLS slot 0. */
inline void osal_cmsis_stop_flag_set(std::atomic<bool>* p) {
    vTaskSetThreadLocalStoragePointer(nullptr, 0, p);
}

#else
/* POSIX/desktop: C++ inline thread_local — exact same semantics. */
inline thread_local std::atomic<bool> *tl_cmsis_stop_flag_storage = nullptr;

inline std::atomic<bool>* osal_cmsis_stop_flag_get() { return tl_cmsis_stop_flag_storage; }
inline void osal_cmsis_stop_flag_set(std::atomic<bool>* p) { tl_cmsis_stop_flag_storage = p; }

/* Backward-compat alias for any code that still uses the old name. */
inline thread_local std::atomic<bool> *&tl_cmsis_stop_flag = tl_cmsis_stop_flag_storage;
#endif

}  // namespace osal
