/** @file osal_thread_snapshot.h
 *  @brief Portable OSAL thread snapshot schema for shell/monitoring use. */
#ifndef DP_OSAL_THREAD_SNAPSHOT_H_
#define DP_OSAL_THREAD_SNAPSHOT_H_

#include <cstdint>

namespace dp::osal {

/** @brief Reason a runtime field is or is not usable for a release claim. */
enum class MeasurementValidity : uint8_t {
    kUnsupported = 0U,
    kValid = 1U,
    kInsufficientWindow = 2U,
    kCounterAmbiguous = 3U,
    kSampleTruncated = 4U,
};

/** @brief Clock domain used for a snapshot's runtime counters. */
enum class MeasurementTimeDomain : uint8_t {
    kUnknown = 0U,
    kFreeRtosRuntimeCounter = 1U,
    kDwtCycles = 2U,
    kPosixProcStatTicks = 3U,
    kMonotonicMicroseconds = 4U,
};

/** @brief Return the elapsed value of a wrapping 32-bit counter. */
constexpr uint32_t runtime_counter_delta_u32(uint32_t newer, uint32_t older) {
    return newer - older;
}

/** @brief Reject a counter interval too large to disambiguate after wrapping. */
constexpr bool runtime_counter_delta_is_bounded(uint32_t newer, uint32_t older, uint32_t max_delta) {
    return runtime_counter_delta_u32(newer, older) <= max_delta && max_delta < 0x80000000U;
}

/** @brief Convert a counter delta to microseconds without overflowing 32 bits. */
constexpr uint32_t runtime_counter_delta_to_us(uint32_t delta, uint32_t counter_hz) {
    return counter_hz == 0U
               ? 0U
               : static_cast<uint32_t>((static_cast<uint64_t>(delta) * 1000000ULL) / counter_hz);
}

struct ThreadSnapshot {
    char name[16]{};
    uint32_t cpu_pct_x10{};  ///< CPU percentage multiplied by ten.
    uint32_t stack_hwm{};    ///< Minimum free stack in bytes.
    uint8_t state{};
    uint32_t stack_pointer{};  ///< 任务保存的栈指针（预缓存用，0 = 未知）
    uint32_t runtime_us{};   ///< Runtime attributed to this thread in the sample window.
    uint32_t isr_runtime_us{};  ///< ISR runtime observed by the port, if available.
    uint32_t sample_window_us{};
    uint32_t sample_sequence{};
    uint32_t counter_hz{};
    uint16_t instrumentation_overhead_us{};
    uint8_t cpu_valid{};
    uint8_t runtime_valid{};
    uint8_t stack_valid{};
    uint8_t isr_valid{};
    uint8_t isr_wrapped{};  ///< Board IRQ wrappers are registered for this sample.
    uint8_t time_domain{static_cast<uint8_t>(MeasurementTimeDomain::kUnknown)};
    uint8_t validity{static_cast<uint8_t>(MeasurementValidity::kUnsupported)};
};

} // namespace dp::osal

#endif  // DP_OSAL_THREAD_SNAPSHOT_H_
