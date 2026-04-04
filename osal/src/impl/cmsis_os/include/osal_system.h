//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include "interface_system.h"
#include "osal.h"
#include "osal_thread_stop.h"

namespace osal {

class OSALSystem : public SystemBase<OSALSystem> {
    friend class SystemBase<OSALSystem>;

public:
    static OSALSystem &getInstance() {
        static OSALSystem instance;
        return instance;
    }

private:
    void doStartScheduler() {
        /* Ensure the kernel is initialized before starting the scheduler.
         * On MCU firmware (dp_stm32f427_dev) osKernelInitialize() is called
         * by the dp_stm32f427 cmsis_os2 port inside osThreadNew() if the
         * kernel is still inactive.  On POSIX simulator products the kernel
         * starts inactive here, so we must call osKernelInitialize() to
         * transition KernelState to osKernelReady before osKernelStart();
         * without this osKernelStart() returns osError immediately and the
         * scheduler never runs, causing the OSALThread destructor to call
         * vPortYield from the main thread and fire vAssertCalled. */
        osKernelInitialize();
        osKernelStart();
    }

    /* Interruptible sleep — mirrors the POSIX backend's cv.wait_for() semantics.
     *
     * Sleeps in SLEEP_CHUNK_MS chunks using osDelay().  After each chunk, the
     * per-thread stop flag is checked.  If set, the sleep is aborted:
     *   - Exception build: throws OSALCmsisThreadStopException (caught by taskRunner)
     *   - Bare-metal / -fno-exceptions: returns early
     *
     * On FreeRTOS (OSAL_PORT_THREAD_FLAGS_WAKE=1), stop() additionally calls
     * osThreadFlagsSet(handle, 0x1) to wake the sleeping thread within 1 ms
     * instead of waiting up to SLEEP_CHUNK_MS ms.  On RT-Thread and Zephyr
     * osThreadFlagsWait is either a stub or requires extra kernel config, so
     * plain osDelay is used and stop latency is bounded to SLEEP_CHUNK_MS.
     *
     * Chunk size (100 ms): under ASan on RT-Thread each osDelay costs ~32 ms
     * of overhead, so 100 ms keeps short sleeps (≤100 ms) within the ±50 ms
     * test tolerance while bounding stop latency to ≤100 ms.
     */
    void doSleepMs(const uint32_t milliseconds) const {
        static constexpr uint32_t SLEEP_CHUNK_MS = 100U;
        uint32_t remaining = milliseconds;
        while (remaining > 0U) {
            uint32_t chunk = (remaining > SLEEP_CHUNK_MS) ? SLEEP_CHUNK_MS : remaining;
            osDelay(chunk);
            remaining -= chunk;
            auto *stop_flag = osal::osal_cmsis_stop_flag_get();
            if (stop_flag != nullptr && stop_flag->load(std::memory_order_acquire)) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                throw OSALCmsisThreadStopException{};
#else
                return;  /* cooperative early return on MCU firmware */
#endif
            }
        }
    }

    void doSleep(const uint32_t seconds) const { doSleepMs(seconds * 1000); }

    [[nodiscard]] const char *doGetSystemInfo() const {
        // Return some basic system information
        return "CMSIS-RTOS2 System";
    }
    OSALSystem(){};

    ~OSALSystem(){};
};

}  // namespace osal
#endif  // OSAL_SYSTEM_H_
