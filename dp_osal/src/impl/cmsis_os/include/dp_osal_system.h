
//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include <cstring>

#include "interface_system.h"
#include "detail/dp_osal_snapshot_detail.h"
#include "dp_osal.h"
#include "dp_osal_thread_stop.h"

namespace dp::osal {


class System : public SystemBase<System> {
    friend class SystemBase<System>;

public:

    static System& getInstance() {
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

    /* 保留现有分片算法：每次最多取数值100并直接传osDelay，返回后检查停止标志。
     * 这里不调用osThreadFlagsWait，也不按内核频率把名义毫秒换成tick，不能据旧注释保证1ms停止。
     * 实际物理时长/停止延迟还依赖tick频率、调度与任务是否继续执行。 */

    void doSleepMs(const uint32_t milliseconds) const {
        static constexpr uint32_t SLEEP_CHUNK_MS = 100U;
        uint32_t remaining = milliseconds;
        while (remaining > 0U) {
            uint32_t chunk = (remaining > SLEEP_CHUNK_MS) ? SLEEP_CHUNK_MS : remaining;
            osDelay(chunk);
            remaining -= chunk;
            auto* stop_flag = osal::osal_cmsis_stop_flag_get();
            if (stop_flag != nullptr && stop_flag->load(std::memory_order_acquire)) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                throw CmsisThreadStopException{};
#else
                return; /* cooperative early return on MCU firmware */
#endif
            }
        }
    }


    void doSleep(const uint32_t seconds) const { doSleepMs(seconds * 1000); }


    [[nodiscard]] uint32_t doGetTickMs() const {
        return static_cast<uint32_t>(osKernelGetTickCount() * 1000U / osKernelGetTickFreq());
    }


    void doEnterCritical() const { (void)osKernelLock(); }

    void doExitCritical() const { (void)osKernelUnlock(); }


    [[nodiscard]] const char* doGetSystemInfo() const {
        // Return some basic system information
        return "CMSIS-RTOS2 System";
    }

#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
#if defined(DP_OSAL_PORT_HAS_RUNTIME_STATS) && DP_OSAL_PORT_HAS_RUNTIME_STATS

    struct RuntimeBaseline {
        void* handle;

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

    using RuntimeRow = DP_OSAL_PORT_RUNTIME_ROW_TYPE;

    static detail::SnapshotCounterStamp runtime_stamp() noexcept {
#if defined(DP_OSAL_PORT_RUNTIME_COUNTER_WIDE_NOW) && defined(DP_OSAL_PORT_RUNTIME_COUNTER_EPOCH)
        const uint32_t frequency = DP_OSAL_PORT_RUNTIME_COUNTER_HZ();
        return {DP_OSAL_PORT_RUNTIME_COUNTER_WIDE_NOW(), frequency, DP_OSAL_PORT_RUNTIME_COUNTER_EPOCH(),
                frequency != 0U};
#else
        return {};
#endif
    }

    static void complete_runtime_window(RuntimeWindow& window, uint32_t total, uint32_t isr, uint32_t previous_total,
                                        uint32_t previous_isr, const detail::SnapshotCounterStamp& previous_before,
                                        const detail::SnapshotCounterStamp& after) noexcept {
        uint64_t bound = 0U;
        window.counter_hz = DP_OSAL_PORT_RUNTIME_COUNTER_HZ();
        const bool bounded = detail::snapshotCounterBound(previous_before, after, window.counter_hz, bound);
        window.total_delta = total - previous_total;
        window.isr_delta = isr - previous_isr;
        window.total_bounded = bounded && window.total_delta <= bound && window.total_delta > 0U &&
                               detail::ticksToUs(window.total_delta, window.counter_hz, window.window_us) &&
                               window.window_us > 0U;
        window.isr_bounded = window.total_bounded && window.isr_delta <= window.total_delta;
    }

    static void initialize_runtime_row(ThreadSnapshot& output, const RuntimeRow& row) noexcept {
        output = ThreadSnapshot{};
        const char* name = row.name == nullptr ? "unknown" : row.name;
        std::strncpy(output.name, name, sizeof(output.name) - 1U);
        output.name[sizeof(output.name) - 1U] = '\0';
        output.stack_hwm = row.stack_hwm_bytes;
        output.stack_pointer = static_cast<uint32_t>(row.stack_pointer);
        output.stack_valid = 1U;
        switch (row.state) {
            case 0U:
                output.state = 1U;
                break;
            case 1U:
                output.state = 2U;
                break;
            case 2U:
                output.state = 3U;
                break;
            case 3U:
                output.state = 4U;
                break;
            default:
                output.state = 5U;
                break;
        }
    }


    [[gnu::noinline]] static void complete_runtime_row(ThreadSnapshot& output, const RuntimeRow& row,
                                                       const RuntimeWindow& window, const RuntimeBaseline* previous,
                                                       size_t previous_count, bool have_window,
                                                       bool first_row) noexcept {
        output.isr_wrapped = window.isr_counter_valid;
        output.sample_window_us = window.window_us;
        output.sample_sequence = window.sample_sequence;
        output.counter_hz = window.counter_hz;
        output.time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kDwtCycles);
        output.validity = static_cast<uint8_t>(MeasurementValidity::kInsufficientWindow);
        bool found = false;
        uint32_t previous_runtime = 0U;
        for (size_t index = 0U; index < previous_count; ++index) {
            if (previous[index].handle == row.handle) {
                found = true;
                previous_runtime = previous[index].runtime_counter;
                break;
            }
        }
        if (!window.truncated && have_window && found && window.total_bounded && window.window_us > 0U) {
            const uint32_t runtime_delta = runtime_counter_delta_u32(row.runtime_counter, previous_runtime);
            uint32_t runtime_us = 0U;
            if (runtime_delta <= window.total_delta &&
                detail::ticksToUs(runtime_delta, window.counter_hz, runtime_us)) {
                output.runtime_us = runtime_us;
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
        if (first_row && !window.truncated && window.isr_counter_valid != 0U && window.isr_bounded && have_window &&
            detail::ticksToUs(window.isr_delta, window.counter_hz, output.isr_runtime_us))
            output.isr_valid = 1U;
        if (window.truncated) {
            output.cpu_valid = 0U;
            output.runtime_valid = 0U;
            output.stack_valid = 0U;
            output.isr_valid = 0U;
            output.validity = static_cast<uint8_t>(MeasurementValidity::kSampleTruncated);
        }
    }

    // 只持有句柄 cursor 与单行 metadata；全部 baseline 查找后才开始覆盖旧表。

#endif


    size_t doGetThreadSnapshot(ThreadSnapshot* buf, size_t max) const {
        if (buf == nullptr || max == 0) {
            return 0;
        }

#if defined(DP_OSAL_PORT_HAS_RUNTIME_STATS) && DP_OSAL_PORT_HAS_RUNTIME_STATS
        static std::atomic_flag gate = ATOMIC_FLAG_INIT;
        detail::SnapshotTryScope sampling(gate);
        if (!sampling) return 0U;
        static RuntimeBaseline previous[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};
        static size_t previous_count = 0U;
        static uint32_t previous_total = 0U, previous_isr = 0U, sequence = 0U;
        static uint8_t previous_isr_valid = 0U;
        static bool have_window = false;
        static detail::SnapshotCounterStamp previous_before{};
        RuntimeRow runtime[DP_OSAL_PORT_RUNTIME_MAX_THREADS]{};
        const size_t capacity = max < DP_OSAL_PORT_RUNTIME_MAX_THREADS ? max : DP_OSAL_PORT_RUNTIME_MAX_THREADS;
        const auto stamp_before = runtime_stamp();
        const uint32_t counter_before = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        uint32_t total_counter = 0U, isr_counter = 0U;
        uint8_t isr_valid = 0U;
        size_t total_count = 0U;
        const size_t returned =
            DP_OSAL_PORT_RUNTIME_GET_THREADS(runtime, capacity, &total_counter, &isr_counter, &isr_valid, &total_count);
        const uint32_t counter_after = DP_OSAL_PORT_RUNTIME_COUNTER_NOW();
        RuntimeWindow window{};
        complete_runtime_window(window, total_counter, isr_counter, previous_total, previous_isr, previous_before,
                                runtime_stamp());
        window.sample_sequence = ++sequence;
        window.truncated = total_count > capacity || returned != total_count || returned > capacity;
        window.isr_counter_valid = isr_valid;
        window.isr_bounded = window.isr_bounded && previous_isr_valid != 0U;
        const size_t got = returned < capacity ? returned : capacity;
        uint32_t overhead_us = 0U;
        (void)detail::ticksToUs(counter_after - counter_before, window.counter_hz, overhead_us);
        for (size_t i = 0U; i < got; ++i) {
            initialize_runtime_row(buf[i], runtime[i]);
            complete_runtime_row(buf[i], runtime[i], window, previous, previous_count, have_window, i == 0U);
            buf[i].instrumentation_overhead_us =
                static_cast<uint16_t>(overhead_us > UINT16_MAX ? UINT16_MAX : overhead_us);
        }
        if (!window.truncated && got > 0U) {
            for (size_t i = 0U; i < got; ++i)
                previous[i] = {runtime[i].handle, runtime[i].runtime_counter};
            previous_count = got;
            previous_total = total_counter;
            previous_isr = isr_counter;
            previous_isr_valid = isr_valid;
            previous_before = stamp_before;
            have_window = stamp_before.valid;
        } else {
            previous_count = 0U;
            have_window = false;
        }
        return got;
#else

        // 无runtime能力时只枚举基础信息，CPU/运行时/ISR标无效；不伪造性能计数。
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
            buf[out] = ThreadSnapshot{};
            const char* name = osThreadGetName(ids[i]);
            if (name == nullptr) {
                name = "unknown";
            }
            std::strncpy(buf[out].name, name, sizeof(buf[out].name) - 1U);
            buf[out].name[sizeof(buf[out].name) - 1U] = '\0';
            // 此回退分支不整体清buf[out]，未覆盖的旧数值必须结合无效标记拒绝使用。
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
            buf[out].validity = static_cast<uint8_t>(sample_truncated ? MeasurementValidity::kSampleTruncated
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

}  // namespace dp::osal
#endif  // OSAL_SYSTEM_H_
