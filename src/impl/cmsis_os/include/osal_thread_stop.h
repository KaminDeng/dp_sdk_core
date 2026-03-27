// osal_thread_stop.h — Cooperative thread-stop support for CMSIS-OS backend.
//
// Provides the stop exception class and a thread-local pointer to the current
// thread's stop-requested flag, so that OSALSystem::sleep_ms() can check
// whether OSALThread::stop() has been called without using FreeRTOS task
// notification flags (which interact poorly with vTaskSuspend/vTaskResume).
#pragma once

#include <atomic>
#include <exception>

namespace osal {

/* Thrown by OSALSystem::sleep_ms() when the current thread's stop flag is set.
 * Caught by OSALThread::taskRunner() so code after the sleep call is skipped. */
struct OSALCmsisThreadStopException : public std::exception {
    const char *what() const noexcept override { return "OSALThread (CMSIS-OS): stop requested"; }
};

/* Thread-local pointer to the running OSALThread's stop flag.
 * Set by taskRunner() before calling the user function; cleared on exit.
 * sleep_ms() reads this to detect stop requests without touching FreeRTOS
 * task notification flags (avoids vTaskSuspend/Resume interaction). */
inline thread_local std::atomic<bool> *tl_cmsis_stop_flag = nullptr;

}  // namespace osal
