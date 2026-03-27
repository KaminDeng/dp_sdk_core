//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include "interface_system.h"
#include "osal.h"
#include "osal_thread_stop.h"

namespace osal {

class OSALSystem : public ISystem {
public:
    static OSALSystem &getInstance() {
        static OSALSystem instance;
        return instance;
    }

    void StartScheduler() override {
        osKernelInitialize();
        osKernelStart();
    }

    /* Interruptible sleep: blocks until 'milliseconds' elapse OR until
     * OSALThread::stop() signals flag 0x1 (stop-requested).
     * When woken by the stop signal, throws OSALCmsisThreadStopException so
     * that any code after the sleep_ms() call in the task function is skipped
     * — matching the POSIX backend's osal_sleep_ms_interruptible() behaviour.
     * The exception is caught by OSALThread::taskRunner(). */
    /* Interruptible sleep: sleeps in chunks that balance stop-response latency
     * against timing accuracy.
     *
     * The chunk size (SLEEP_CHUNK_MS) controls the trade-off:
     *  - Small chunks (1 ms): fine stop-response, but many sem_wait/sem_post
     *    round-trips.  Under ASan on RT-Thread each sem_wait call costs ~32 ms
     *    of extra overhead (ASan instrumentation + context-switch), making a
     *    1-ms osDelay take ~32 ms — 32× slower than expected.
     *  - Large chunks (e.g. 100 ms): accurate timing (one sem_wait per chunk),
     *    but stop() may not be detected for up to SLEEP_CHUNK_MS milliseconds.
     *
     * 100 ms is a good default: on FreeRTOS ASAN (pthread_cond_wait, no overhead)
     * accuracy is fine; on RT-Thread ASAN only one sem_wait per chunk so the
     * wall-clock deviation is bounded to ≈(overhead_per_sem_wait = ~32 ms) per
     * chunk, keeping short sleeps (≤ 100 ms) within the test tolerance of ±50 ms.
     */
    void sleep_ms(const uint32_t milliseconds) const override {
        static constexpr uint32_t SLEEP_CHUNK_MS = 100U;
        uint32_t remaining = milliseconds;
        while (remaining > 0U) {
            uint32_t chunk = (remaining > SLEEP_CHUNK_MS) ? SLEEP_CHUNK_MS : remaining;
            osDelay(chunk);
            remaining -= chunk;
            if (tl_cmsis_stop_flag != nullptr && tl_cmsis_stop_flag->load(std::memory_order_acquire)) {
                throw OSALCmsisThreadStopException{};
            }
        }
    }

    void sleep(const uint32_t seconds) const override { sleep_ms(seconds * 1000); }

    [[nodiscard]] const char *get_system_info() const override {
        // Return some basic system information
        return "CMSIS-RTOS2 System";
    }

private:
    OSALSystem(){};

    ~OSALSystem(){};
};

}  // namespace osal
#endif  // OSAL_SYSTEM_H_
