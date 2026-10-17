//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include <cstring>

#include "interface_system.h"
#include "dp_osal.h"
#include "dp_osal_thread_stop.h"

namespace dp::osal {

class System : public SystemBase<System> {
    friend class SystemBase<System>;

public:
    static System &getInstance() {
        static System instance;
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
         * scheduler never runs, causing the Thread destructor to call
         * vPortYield from the main thread and fire vAssertCalled. */
        osKernelInitialize();
        osKernelStart();
    }

    /* Interruptible sleep — mirrors the POSIX backend's cv.wait_for() semantics.
     *
     * Sleeps in SLEEP_CHUNK_MS chunks using osDelay().  After each chunk, the
     * per-thread stop flag is checked.  If set, the sleep is aborted:
     *   - Exception build: throws CmsisThreadStopException (caught by taskRunner)
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
                throw CmsisThreadStopException{};
#else
                return;  /* cooperative early return on MCU firmware */
#endif
            }
        }
    }

    void doSleep(const uint32_t seconds) const { doSleepMs(seconds * 1000); }

    [[nodiscard]] uint32_t doGetTickMs() const {
        return static_cast<uint32_t>(static_cast<uint64_t>(osKernelGetTickCount()) * 1000ULL / osKernelGetTickFreq());
    }

    void doEnterCritical() const {
        (void)osKernelLock();
    }
    void doExitCritical() const {
        (void)osKernelUnlock();
    }

    [[nodiscard]] const char *doGetSystemInfo() const {
        // Return some basic system information
        return "CMSIS-RTOS2 System";
    }

#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STATS) && DP_OSAL_PORT_HAS_RUNTIME_STATS && \
    defined(DP_OSAL_PORT_HAS_RUNTIME_STREAM) && DP_OSAL_PORT_HAS_RUNTIME_STREAM
    struct RuntimeBaseline {
        void* handle;
        uint32_t task_generation;
        uint32_t runtime_counter;
    };
    struct RuntimeWindow {
        uint32_t total_delta{};
        uint32_t window_us{};
        uint32_t isr_delta{};
        uint32_t counter_hz{};
        uint32_t sample_sequence{};
        bool total_bounded{};
        bool isr_bounded{};
        bool truncated{};
        uint8_t isr_counter_valid{};
    };
    using RuntimeCursor = DP_OSAL_PORT_RUNTIME_CURSOR_TYPE;
    using RuntimeRow = DP_OSAL_PORT_RUNTIME_ROW_TYPE;

    class RuntimeScope {
    public:
        explicit RuntimeScope(RuntimeCursor& cursor) noexcept : cursor_(&cursor) {}
        RuntimeScope(const RuntimeScope&) = delete;
        RuntimeScope& operator=(const RuntimeScope&) = delete;
        ~RuntimeScope() { finish(); }
        void finish() noexcept {
            if (cursor_ != nullptr) {
                DP_OSAL_PORT_RUNTIME_END(cursor_);
                cursor_ = nullptr;
            }
        }
    private:
        RuntimeCursor* cursor_;
    };

    static void initialize_runtime_row(ThreadSnapshot& output, const RuntimeRow& row) noexcept {
        output = ThreadSnapshot{};
        const char* name = row.name == nullptr ? "unknown" : row.name;
        std::strncpy(output.name, name, sizeof(output.name) - 1U);
        output.name[sizeof(output.name) - 1U] = '\0';
        output.stack_hwm = row.stack_hwm_bytes;
        output.stack_pointer = static_cast<uint32_t>(row.stack_pointer);
        output.stack_valid = 1U;
        switch (row.state) {
            case 0U: output.state = 1U; break;
            case 1U: output.state = 2U; break;
            case 2U: output.state = 3U; break;
            case 3U: output.state = 4U; break;
            default: output.state = 5U; break;
        }
    }

    [[gnu::noinline]] static void complete_runtime_row(
        ThreadSnapshot& output, const RuntimeRow& row, const RuntimeWindow& window,
        const RuntimeBaseline* previous, size_t previous_count,
        bool have_window, bool first_row) noexcept {
        output.isr_wrapped = window.isr_counter_valid;
        output.sample_window_us = window.window_us;
        output.sample_sequence = window.sample_sequence;
        output.counter_hz = window.counter_hz;
#ifdef DP_OSAL_PORT_RUNTIME_TIME_DOMAIN
        output.time_domain = static_cast<uint8_t>(DP_OSAL_PORT_RUNTIME_TIME_DOMAIN);
#else
        output.time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kFreeRtosRuntimeCounter);
#endif
        output.validity = static_cast<uint8_t>(MeasurementValidity::kInsufficientWindow);
        bool found = false;
        uint32_t previous_runtime = 0U;
        for (size_t index = 0U; index < previous_count; ++index) {
            if (previous[index].handle == row.handle &&
                previous[index].task_generation == row.task_generation) {
                found = true;
                previous_runtime = previous[index].runtime_counter;
                break;
            }
        }
        if (!window.truncated && have_window && found &&
            window.total_bounded && window.window_us > 0U) {
            const uint32_t runtime_delta = runtime_counter_delta_u32(row.runtime_counter, previous_runtime);
            if (runtime_counter_delta_is_bounded(row.runtime_counter, previous_runtime, 0x7FFFFFFFU)) {
                output.runtime_us = runtime_counter_delta_to_us(runtime_delta, window.counter_hz);
                const uint64_t percent = (static_cast<uint64_t>(runtime_delta) * 1000ULL) /
                    static_cast<uint64_t>(window.total_delta == 0U ? 1U : window.total_delta);
                output.cpu_pct_x10 = static_cast<uint32_t>(percent > 1000ULL ? 1000ULL : percent);
                output.runtime_valid = 1U;
                output.cpu_valid = 1U;
                output.validity = static_cast<uint8_t>(MeasurementValidity::kValid);
            } else {
                output.validity = static_cast<uint8_t>(MeasurementValidity::kCounterAmbiguous);
            }
        }
        if (first_row && window.isr_counter_valid != 0U && window.isr_bounded && have_window) {
            output.isr_runtime_us = runtime_counter_delta_to_us(window.isr_delta, window.counter_hz);
            output.isr_valid = 1U;
        }
        if (window.truncated) {
            output.cpu_valid = 0U;
            output.runtime_valid = 0U;
            output.stack_valid = 0U;
            output.isr_valid = 0U;
            output.validity = static_cast<uint8_t>(MeasurementValidity::kSampleTruncated);
        }
    }

    // 只持有句柄 cursor 与单行 metadata；全部 baseline 查找后才开始覆盖旧表。
    [[gnu::noinline]] size_t doGetThreadSnapshotStream(ThreadSnapshot* output, size_t max) const {
        static RuntimeBaseline previous[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};
        static size_t previous_count = 0U;
        static uint32_t previous_total = 0U;
        static uint32_t previous_isr = 0U;
        static uint32_t sequence = 0U;
        static bool have_window = false;
        RuntimeCursor cursor{};
        RuntimeRow row{};
        const size_t capacity = max < DP_OSAL_PORT_RUNTIME_MAX_THREADS
            ? max : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
        const uint32_t counter_before = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        size_t total_count = 0U;
        const size_t enumerated = DP_OSAL_PORT_RUNTIME_BEGIN(&cursor, &total_count);
        RuntimeScope scope(cursor);
        const uint32_t sample_sequence = ++sequence;
        const size_t got = enumerated < capacity ? enumerated : capacity;
        for (size_t index = 0U; index < enumerated; ++index) {
            if (!DP_OSAL_PORT_RUNTIME_READ(&cursor, index, 1U, &row)) {
                previous_count = 0U;
                have_window = false;
                return 0U;
            }
            if (index < got) initialize_runtime_row(output[index], row);
        }
        uint32_t total_counter = 0U;
        uint32_t isr_counter = 0U;
        uint8_t isr_counter_valid = 0U;
        // 保留原全体 HWM 扫描结束后的 counter 采样点。
        DP_OSAL_PORT_RUNTIME_COUNTERS(&cursor, &total_counter, &isr_counter, &isr_counter_valid);
        RuntimeWindow window{};
        window.counter_hz = DP_OSAL_PORT_RUNTIME_COUNTER_HZ();
        window.total_delta = runtime_counter_delta_u32(total_counter, previous_total);
        window.total_bounded = runtime_counter_delta_is_bounded(total_counter, previous_total, 0x7FFFFFFFU);
        window.window_us = window.total_bounded
            ? runtime_counter_delta_to_us(window.total_delta, window.counter_hz) : 0U;
        window.isr_delta = runtime_counter_delta_u32(isr_counter, previous_isr);
        window.isr_bounded = runtime_counter_delta_is_bounded(isr_counter, previous_isr, 0x7FFFFFFFU);
        window.sample_sequence = sample_sequence;
        window.truncated = total_count > capacity || got < total_count;
        window.isr_counter_valid = isr_counter_valid;
        for (size_t index = 0U; index < got; ++index) {
            if (!DP_OSAL_PORT_RUNTIME_READ(&cursor, index, 0U, &row)) {
                previous_count = 0U;
                have_window = false;
                return 0U;
            }
            complete_runtime_row(output[index], row, window, previous, previous_count,
                                 have_window, index == 0U);
        }
        for (size_t index = 0U; index < got; ++index) {
            if (!DP_OSAL_PORT_RUNTIME_READ(&cursor, index, 0U, &row)) {
                previous_count = 0U;
                have_window = false;
                return 0U;
            }
            previous[index] = {row.handle, row.task_generation, row.runtime_counter};
        }
        previous_count = got;
        previous_total = total_counter;
        previous_isr = isr_counter;
        have_window = !window.truncated && window.total_bounded && window.window_us > 0U;
        scope.finish();
        const uint32_t counter_after = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        const uint32_t overhead_us = runtime_counter_delta_to_us(
            counter_after - counter_before, window.counter_hz);
        const uint16_t overhead = static_cast<uint16_t>(overhead_us > 0xFFFFU ? 0xFFFFU : overhead_us);
        for (size_t index = 0U; index < got; ++index)
            output[index].instrumentation_overhead_us = overhead;
        return got;
    }
#endif

    size_t doGetThreadSnapshot(ThreadSnapshot *buf, size_t max) const {
        if (buf == nullptr || max == 0) {
            return 0;
        }

#if defined(DP_OSAL_PORT_HAS_RUNTIME_STATS) && DP_OSAL_PORT_HAS_RUNTIME_STATS
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STREAM) && DP_OSAL_PORT_HAS_RUNTIME_STREAM
        return doGetThreadSnapshotStream(buf, max);
#else
        /* Runtime stats are collected through a bounded port callback.  The
         * OSAL layer owns the validity/window arithmetic; application code
         * never needs FreeRTOS task handles or counters. */
        struct RuntimeBaseline {
            void *handle;
            uint32_t task_generation;
            uint32_t runtime_counter;
        };
        static RuntimeBaseline previous[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};
        static size_t previous_count = 0U;
        static uint32_t previous_total = 0U;
        static uint32_t previous_isr = 0U;
        static uint32_t sequence = 0U;
        static bool have_window = false;

        dp_f405_thread_runtime_t runtime[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};
        const size_t capacity = max < DP_OSAL_PORT_RUNTIME_MAX_THREADS
                                    ? max
                                    : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
        const uint32_t counter_before = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        uint32_t total_counter = 0U;
        uint32_t isr_counter = 0U;
        uint8_t isr_counter_valid = 0U;
        size_t total_count = 0U;
        const size_t got = DP_OSAL_PORT_RUNTIME_GET_THREADS(runtime, capacity, &total_counter,
                                                            &isr_counter, &isr_counter_valid,
                                                            &total_count);
        const uint32_t counter_after = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        const uint32_t counter_hz = DP_OSAL_PORT_RUNTIME_COUNTER_HZ();
        const uint32_t instrumentation_counter = counter_after - counter_before;
        const uint32_t total_delta = runtime_counter_delta_u32(total_counter, previous_total);
        const bool total_bounded = runtime_counter_delta_is_bounded(total_counter, previous_total,
                                                                    0x7FFFFFFFU);
        const uint32_t window_us = total_bounded
                                       ? runtime_counter_delta_to_us(total_delta, counter_hz)
                                       : 0U;
        const uint32_t isr_delta = runtime_counter_delta_u32(isr_counter, previous_isr);
        const bool isr_bounded = runtime_counter_delta_is_bounded(isr_counter, previous_isr,
                                                                  0x7FFFFFFFU);
        const uint32_t overhead_us = runtime_counter_delta_to_us(instrumentation_counter, counter_hz);
        const uint32_t sample_sequence = ++sequence;
        const uint32_t sample_overhead = overhead_us > 0xFFFFU ? 0xFFFFU : overhead_us;
        const bool sample_truncated = total_count > capacity || got < total_count;

        size_t out = 0U;
        for (size_t i = 0U; i < got && out < max; ++i, ++out) {
            // 输出可由调用者长期复用；无效窗口不能继承上一轮有效标记或计量值。
            buf[out] = ThreadSnapshot{};
            const char *name = runtime[i].name == nullptr ? "unknown" : runtime[i].name;
            std::strncpy(buf[out].name, name, sizeof(buf[out].name) - 1U);
            buf[out].name[sizeof(buf[out].name) - 1U] = '\0';
            buf[out].stack_hwm = runtime[i].stack_hwm_bytes;
            buf[out].stack_pointer = static_cast<uint32_t>(runtime[i].stack_pointer);
            buf[out].stack_valid = 1U;
            buf[out].isr_wrapped = isr_counter_valid;
            buf[out].sample_window_us = window_us;
            buf[out].sample_sequence = sample_sequence;
            buf[out].counter_hz = counter_hz;
            buf[out].instrumentation_overhead_us = static_cast<uint16_t>(sample_overhead);
#ifdef DP_OSAL_PORT_RUNTIME_TIME_DOMAIN
            buf[out].time_domain = static_cast<uint8_t>(DP_OSAL_PORT_RUNTIME_TIME_DOMAIN);
#else
            buf[out].time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kFreeRtosRuntimeCounter);
#endif
            buf[out].validity = static_cast<uint8_t>(MeasurementValidity::kInsufficientWindow);
            switch (runtime[i].state) {
                case 0U:
                    buf[out].state = 1U;
                    break;
                case 1U:
                    buf[out].state = 2U;
                    break;
                case 2U:
                    buf[out].state = 3U;
                    break;
                case 3U:
                    buf[out].state = 4U;
                    break;
                default:
                    buf[out].state = 5U;
                    break;
            }

            bool found = false;
            uint32_t previous_runtime = 0U;
            for (size_t j = 0U; j < previous_count; ++j) {
                if (previous[j].handle == runtime[i].handle &&
                    previous[j].task_generation == runtime[i].task_generation) {
                    found = true;
                    previous_runtime = previous[j].runtime_counter;
                    break;
                }
            }
            if (!sample_truncated && have_window && found && total_bounded && window_us > 0U) {
                const uint32_t runtime_delta = runtime_counter_delta_u32(runtime[i].runtime_counter,
                                                                          previous_runtime);
                if (runtime_counter_delta_is_bounded(runtime[i].runtime_counter, previous_runtime,
                                                     0x7FFFFFFFU)) {
                    const uint32_t runtime_us = runtime_counter_delta_to_us(runtime_delta, counter_hz);
                    const uint64_t pct_x10 = (static_cast<uint64_t>(runtime_delta) * 1000ULL) /
                                             static_cast<uint64_t>(total_delta == 0U ? 1U : total_delta);
                    buf[out].runtime_us = runtime_us;
                    buf[out].cpu_pct_x10 = static_cast<uint32_t>(pct_x10 > 1000ULL ? 1000ULL : pct_x10);
                    buf[out].runtime_valid = 1U;
                    buf[out].cpu_valid = 1U;
                    buf[out].validity = static_cast<uint8_t>(MeasurementValidity::kValid);
                } else {
                    buf[out].validity = static_cast<uint8_t>(MeasurementValidity::kCounterAmbiguous);
                }
            }
            if (out == 0U && isr_counter_valid != 0U && isr_bounded && have_window) {
                buf[out].isr_runtime_us = runtime_counter_delta_to_us(isr_delta, counter_hz);
                buf[out].isr_valid = 1U;
            }
        }

        if (sample_truncated) {
            for (size_t i = 0U; i < out; ++i) {
                buf[i].cpu_valid = 0U;
                buf[i].runtime_valid = 0U;
                buf[i].stack_valid = 0U;
                buf[i].isr_valid = 0U;
                buf[i].validity = static_cast<uint8_t>(MeasurementValidity::kSampleTruncated);
            }
        }

        const size_t next_count = got < DP_OSAL_PORT_RUNTIME_MAX_THREADS
                                      ? got
                                      : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
        for (size_t i = 0U; i < next_count; ++i) {
            previous[i].handle = runtime[i].handle;
            previous[i].task_generation = runtime[i].task_generation;
            previous[i].runtime_counter = runtime[i].runtime_counter;
        }
        previous_count = next_count;
        previous_total = total_counter;
        previous_isr = isr_counter;
        have_window = !sample_truncated && total_bounded && window_us > 0U;
        return out;
#endif
#else

        uint32_t count = osThreadGetCount();
        if (count == 0) {
            return 0;
        }

        osThreadId_t ids[32] = {nullptr};
        const bool sample_truncated = count > max || count > 32U;
        uint32_t cap = count;
        if (cap > max) {
            cap = static_cast<uint32_t>(max);
        }
        if (cap > 32U) {
            cap = 32U;
        }
        uint32_t got = osThreadEnumerate(ids, cap);

        size_t out = 0;
        for (uint32_t i = 0; i < got && out < max; ++i, ++out) {
            const char *name = osThreadGetName(ids[i]);
            if (name == nullptr) {
                name = "unknown";
            }
            std::strncpy(buf[out].name, name, sizeof(buf[out].name) - 1U);
            buf[out].name[sizeof(buf[out].name) - 1U] = '\0';
            buf[out].cpu_pct_x10 = 0U;
            buf[out].stack_hwm = osThreadGetStackSpace(ids[i]);
            buf[out].state = static_cast<uint8_t>(osThreadGetState(ids[i]));
            buf[out].stack_pointer = DP_OSAL_PORT_THREAD_STACK_POINTER_FROM_ID(ids[i]);
            buf[out].cpu_valid = 0U;
            buf[out].runtime_valid = 0U;
            buf[out].stack_valid = 1U;
            buf[out].isr_valid = 0U;
            buf[out].isr_wrapped = 0U;
            buf[out].time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kUnknown);
            buf[out].validity = static_cast<uint8_t>(sample_truncated
                                                         ? MeasurementValidity::kSampleTruncated
                                                         : MeasurementValidity::kUnsupported);
            if (sample_truncated) {
                buf[out].stack_valid = 0U;
            }
        }
        return out;
#endif
    }
#endif

    System(){};

    ~System(){};
};

} // namespace dp::osal
#endif  // OSAL_SYSTEM_H_
