/** @file dp_osal_measurement.h
 *  @brief Bounded OSAL synchronization timing counters.
 *
 * The counters deliberately aggregate instead of retaining an unbounded event
 * log.  A caller can reset, run one bounded workload, and snapshot the result
 * through the OSAL interface without exposing RTOS handles to application code.
 */
#ifndef DP_OSAL_MEASUREMENT_H_
#define DP_OSAL_MEASUREMENT_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>

#include "dp_osal_thread_snapshot.h"

/* Keep the weak hook in the global namespace so C ports can define it without
 * including a C++ namespace. */
#if defined(__GNUC__)
extern "C" uint32_t dp_osal_port_measurement_now_us(void) __attribute__((weak));
#else
extern "C" uint32_t dp_osal_port_measurement_now_us(void);
#endif

namespace dp::osal {

enum class SynchronizationKind : uint8_t {
    kLock = 0U,
    kWake = 1U,
    kQueue = 2U,
};

struct SynchronizationMeasurement {
    uint32_t lock_wait_count{};
    uint32_t lock_contended_count{};
    uint32_t lock_wait_total_us{};
    uint32_t lock_wait_max_us{};
    uint32_t wake_wait_count{};
    uint32_t wake_wait_total_us{};
    uint32_t wake_wait_max_us{};
    uint32_t wake_signal_count{};
    uint32_t wake_isr_signal_count{};
    uint32_t queue_wait_count{};
    uint32_t queue_wait_total_us{};
    uint32_t queue_wait_max_us{};
    uint32_t instrumentation_overhead_us{};
    uint32_t sample_sequence{};
    uint8_t valid{};
    uint8_t overflow{};
    uint8_t instrumentation_enabled{};
    uint8_t time_domain{static_cast<uint8_t>(MeasurementTimeDomain::kMonotonicMicroseconds)};
};

/* A weak MCU hook lets a port use its free-running counter without making
 * interface headers depend on a specific HAL or FreeRTOS header. */
class SynchronizationMeasurementCollector {
public:
    static uint32_t now_us() {
        if (dp_osal_port_measurement_now_us != nullptr) {
            return dp_osal_port_measurement_now_us();
        }
#if defined(DPSDK_MCU_FIRMWARE)
        return 0U;
#else
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
#endif
    }

    static void set_enabled(bool enabled) { enabled_.store(enabled, std::memory_order_release); }
    static bool enabled() { return enabled_.load(std::memory_order_acquire); }

    static void reset() {
        lock_wait_count_.store(0U);
        lock_contended_count_.store(0U);
        lock_wait_total_us_.store(0U);
        lock_wait_max_us_.store(0U);
        wake_wait_count_.store(0U);
        wake_wait_total_us_.store(0U);
        wake_wait_max_us_.store(0U);
        wake_signal_count_.store(0U);
        wake_isr_signal_count_.store(0U);
        queue_wait_count_.store(0U);
        queue_wait_total_us_.store(0U);
        queue_wait_max_us_.store(0U);
        instrumentation_overhead_us_.store(0U);
        overflow_.store(false);
        sample_sequence_.store(0U);
    }

    static SynchronizationMeasurement snapshot() {
        SynchronizationMeasurement out{};
        out.lock_wait_count = lock_wait_count_.load();
        out.lock_contended_count = lock_contended_count_.load();
        out.lock_wait_total_us = lock_wait_total_us_.load();
        out.lock_wait_max_us = lock_wait_max_us_.load();
        out.wake_wait_count = wake_wait_count_.load();
        out.wake_wait_total_us = wake_wait_total_us_.load();
        out.wake_wait_max_us = wake_wait_max_us_.load();
        out.wake_signal_count = wake_signal_count_.load();
        out.wake_isr_signal_count = wake_isr_signal_count_.load();
        out.queue_wait_count = queue_wait_count_.load();
        out.queue_wait_total_us = queue_wait_total_us_.load();
        out.queue_wait_max_us = queue_wait_max_us_.load();
        out.instrumentation_overhead_us = instrumentation_overhead_us_.load();
        out.sample_sequence = sample_sequence_.load();
        out.overflow = overflow_.load() ? 1U : 0U;
        out.instrumentation_enabled = enabled() ? 1U : 0U;
        out.valid = (out.instrumentation_enabled != 0U && out.overflow == 0U) ? 1U : 0U;
        return out;
    }

    static void record_wait(SynchronizationKind kind, uint32_t started_at_us, uint32_t finished_at_us,
                            bool acquired) {
        if (!enabled()) return;
        const uint32_t overhead_started = now_us();
        const uint32_t elapsed = runtime_counter_delta_u32(finished_at_us, started_at_us);
        if (kind == SynchronizationKind::kLock) {
            add(lock_wait_count_, 1U);
            if (!acquired) add(lock_contended_count_, 1U);
            add(lock_wait_total_us_, elapsed);
            update_max(lock_wait_max_us_, elapsed);
        } else if (kind == SynchronizationKind::kWake) {
            add(wake_wait_count_, 1U);
            add(wake_wait_total_us_, elapsed);
            update_max(wake_wait_max_us_, elapsed);
        } else {
            add(queue_wait_count_, 1U);
            add(queue_wait_total_us_, elapsed);
            update_max(queue_wait_max_us_, elapsed);
        }
        add(sample_sequence_, 1U);
        add(instrumentation_overhead_us_, runtime_counter_delta_u32(now_us(), overhead_started));
    }

    static void record_signal(bool from_isr) {
        if (!enabled()) return;
        const uint32_t overhead_started = now_us();
        add(wake_signal_count_, 1U);
        if (from_isr) add(wake_isr_signal_count_, 1U);
        add(sample_sequence_, 1U);
        add(instrumentation_overhead_us_, runtime_counter_delta_u32(now_us(), overhead_started));
    }

    static void record_instrumentation(uint32_t started_at_us, uint32_t finished_at_us) {
        if (!enabled()) return;
        add(instrumentation_overhead_us_, runtime_counter_delta_u32(finished_at_us, started_at_us));
    }

private:
    static void add(std::atomic<uint32_t> &target, uint32_t value) {
        uint32_t current = target.load(std::memory_order_relaxed);
        for (;;) {
            if (std::numeric_limits<uint32_t>::max() - current < value) {
                overflow_.store(true, std::memory_order_release);
                current = std::numeric_limits<uint32_t>::max();
            } else if (target.compare_exchange_weak(current, current + value,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed)) {
                return;
            }
            if (current == std::numeric_limits<uint32_t>::max()) return;
        }
    }

    static void update_max(std::atomic<uint32_t> &target, uint32_t value) {
        uint32_t current = target.load(std::memory_order_relaxed);
        while (current < value && !target.compare_exchange_weak(current, value,
                                                                std::memory_order_relaxed,
                                                                std::memory_order_relaxed)) {
        }
    }

    inline static std::atomic<bool> enabled_{true};
    inline static std::atomic<bool> overflow_{false};
    inline static std::atomic<uint32_t> lock_wait_count_{0U};
    inline static std::atomic<uint32_t> lock_contended_count_{0U};
    inline static std::atomic<uint32_t> lock_wait_total_us_{0U};
    inline static std::atomic<uint32_t> lock_wait_max_us_{0U};
    inline static std::atomic<uint32_t> wake_wait_count_{0U};
    inline static std::atomic<uint32_t> wake_wait_total_us_{0U};
    inline static std::atomic<uint32_t> wake_wait_max_us_{0U};
    inline static std::atomic<uint32_t> wake_signal_count_{0U};
    inline static std::atomic<uint32_t> wake_isr_signal_count_{0U};
    inline static std::atomic<uint32_t> queue_wait_count_{0U};
    inline static std::atomic<uint32_t> queue_wait_total_us_{0U};
    inline static std::atomic<uint32_t> queue_wait_max_us_{0U};
    inline static std::atomic<uint32_t> instrumentation_overhead_us_{0U};
    inline static std::atomic<uint32_t> sample_sequence_{0U};
};

using Measurement = SynchronizationMeasurementCollector;

}  // namespace dp::osal

#endif  // DP_OSAL_MEASUREMENT_H_
