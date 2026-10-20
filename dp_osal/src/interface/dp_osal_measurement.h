/** @file dp_osal_measurement.h
 * @brief 同步诊断兼容入口。宽采样与质量判定在私有 detail；关闭时没有采集存储。
 * @details reset 前必须停止并等待所有使用者；关闭开关不是在途采样屏障。
 * 快照逐字段读取，不提供事务一致性。lock_contended_count 保持获取失败次数语义。 */
#ifndef DP_OSAL_MEASUREMENT_H_
#define DP_OSAL_MEASUREMENT_H_
#include <cstdint>
#include "dp_osal_thread_snapshot.h"
#ifndef DP_OSAL_ENABLE_MEASUREMENT
#define DP_OSAL_ENABLE_MEASUREMENT 0
#endif
#if DP_OSAL_ENABLE_MEASUREMENT
#include <atomic>
#include <limits>
#include "detail/dp_osal_measurement_detail.h"
#if __has_include("dp_osal_measurement_clock.h")
#include "dp_osal_measurement_clock.h"
#else
#include "detail/dp_osal_measurement_clock_fallback.h"
#endif
#endif
// 旧32位短计时hook仍可链接；仅它本身不能赋予长等待采样有效资格。
#if defined(__GNUC__)
extern "C" uint32_t dp_osal_port_measurement_now_us(void) __attribute__((weak));
#else
extern "C" uint32_t dp_osal_port_measurement_now_us(void);
#endif
namespace dp::osal {
enum class SynchronizationKind : uint8_t { kLock = 0U, kWake = 1U, kQueue = 2U };
struct SynchronizationMeasurement {
    uint32_t lock_wait_count{}; ///< 锁获取包装的记录次数，包含成功与失败。
    uint32_t lock_contended_count{}; ///< acquired=false的锁调用次数；成功但确实等待过的调用不计入此字段。
    uint32_t lock_wait_total_us{}; ///< 锁获取包装耗时之和，单位微秒，包含底层整个调用。
    uint32_t lock_wait_max_us{}; ///< 单次锁获取包装的最大观测耗时，微秒。
    uint32_t wake_wait_count{}; ///< 信号量等待包装的记录次数，不独立区分成功与失败。
    uint32_t wake_wait_total_us{}; ///< 等待包装耗时累计，微秒。
    uint32_t wake_wait_max_us{}; ///< 等待包装最大观测耗时，微秒。
    uint32_t wake_signal_count{}; ///< 线程与ISR发送包装的总记录次数，不等于成功唤醒线程数。
    uint32_t wake_isr_signal_count{}; ///< from_isr=true的发送记录次数，是总发送次数的子集。
    uint32_t queue_wait_count{}; ///< 队列发送/接收包装的记录次数。
    uint32_t queue_wait_total_us{}; ///< 队列包装耗时累计，微秒。
    uint32_t queue_wait_max_us{}; ///< 队列包装最大观测耗时，微秒。
    uint32_t instrumentation_overhead_us{}; ///< 收集器所观测的部分插桩开销累计，微秒，不是全链路精确校准值。
    uint32_t sample_sequence{}; ///< record_wait/record_signal记录次数，不是保护整份快照的seqlock版本。
    uint8_t valid{}; ///< 仅当启用、完整范围受支持且所有时间样本有效未溢出时为1；不是事务快照。
    uint8_t overflow{}; ///< 某次累加超出uint32_t范围后锁存为1，reset清除。
    uint8_t instrumentation_enabled{}; ///< 快照时开关是否启用；关闭并不清空旧统计。
    uint8_t time_domain{static_cast<uint8_t>(MeasurementTimeDomain::kMonotonicMicroseconds)}; ///< 声明计数以单调微秒为时间域，不证明旧32位hook质量。
};

class SynchronizationMeasurementCollector {
public:
    static uint32_t now_us() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (dp_osal_port_measurement_now_us != nullptr) return dp_osal_port_measurement_now_us();
        return static_cast<uint32_t>(detail::measurementNow().us);
#else
        return 0U;
#endif
    }
    static bool clock_available() noexcept {
#if DP_OSAL_ENABLE_MEASUREMENT
        return detail::measurementClockAvailable();
#else
        return false;
#endif
    }
    // 只发布开关，不等待在途事件；重新启用沿用旧计数。
    static void set_enabled(bool value) {
#if DP_OSAL_ENABLE_MEASUREMENT
        enabled_.store(value, std::memory_order_release);
#else
        (void)value;
#endif
    }
    static bool enabled() {
#if DP_OSAL_ENABLE_MEASUREMENT
        return enabled_.load(std::memory_order_acquire);
#else
        return false;
#endif
    }
    // 调用者先停止并等待全部记录者；reset 不是并发屏障。
    static void reset() {
#if DP_OSAL_ENABLE_MEASUREMENT
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
        sample_sequence_.store(0U);
        overflow_.store(false);
        invalid_.store(false);
#endif
    }
    static SynchronizationMeasurement snapshot() {
        SynchronizationMeasurement out{};
#if DP_OSAL_ENABLE_MEASUREMENT
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
        out.valid = (out.instrumentation_enabled && !out.overflow && !invalid_.load() &&
                     clock_available() && detail::measurementThreadIsrTag()) ? 1U : 0U;
#endif
        return out;
    }
    // 旧32位参数没有持续时间上界证明：保留计数，整份结果无效，不猜测回绕次数。
    static void record_wait(SynchronizationKind kind, uint32_t started_at_us,
                            uint32_t finished_at_us, bool acquired) {
        (void)started_at_us; (void)finished_at_us;
#if DP_OSAL_ENABLE_MEASUREMENT
        if (!enabled()) return;
        invalid_.store(true);
        recordWait(kind, {}, {}, acquired);
#else
        (void)kind; (void)acquired;
#endif
    }
    static void record_signal(bool from_isr) {
#if DP_OSAL_ENABLE_MEASUREMENT
        // 非线程ISR标签不采样、不CAS；固定能力已使整份快照无效。
        if (from_isr && !detail::measurementThreadIsrTag()) return;
        if (!enabled()) return;
        const auto begin = detail::measurementNow();
        add(wake_signal_count_, 1U);
        if (from_isr) add(wake_isr_signal_count_, 1U);
        add(sample_sequence_, 1U);
        recordOverhead(begin, detail::measurementNow());
#else
        (void)from_isr;
#endif
    }
    static void record_instrumentation(uint32_t started_at_us, uint32_t finished_at_us) {
        (void)started_at_us; (void)finished_at_us;
#if DP_OSAL_ENABLE_MEASUREMENT
        if (enabled()) invalid_.store(true);
#endif
    }
private:
#if DP_OSAL_ENABLE_MEASUREMENT
    friend struct detail::MeasurementAccess;
    static void recordWait(SynchronizationKind kind, const detail::MeasurementStamp& begin,
                           const detail::MeasurementStamp& end, bool acquired) {
        if (!enabled()) return;
        const auto overhead_begin = detail::measurementNow();
        std::atomic<uint32_t>* count = &queue_wait_count_;
        std::atomic<uint32_t>* total = &queue_wait_total_us_;
        std::atomic<uint32_t>* maximum = &queue_wait_max_us_;
        if (kind == SynchronizationKind::kLock) {
            count = &lock_wait_count_; total = &lock_wait_total_us_; maximum = &lock_wait_max_us_;
            if (!acquired) add(lock_contended_count_, 1U);
        } else if (kind == SynchronizationKind::kWake) {
            count = &wake_wait_count_; total = &wake_wait_total_us_; maximum = &wake_wait_max_us_;
        }
        add(*count, 1U);
        uint64_t elapsed = 0U;
        if (!detail::elapsedUs(begin, end, elapsed)) invalid_.store(true);
        else if (elapsed > std::numeric_limits<uint32_t>::max()) overflow_.store(true);
        else { add(*total, static_cast<uint32_t>(elapsed)); updateMax(*maximum, static_cast<uint32_t>(elapsed)); }
        add(sample_sequence_, 1U);
        recordOverhead(overhead_begin, detail::measurementNow());
    }
    static void recordOverhead(const detail::MeasurementStamp& begin,
                               const detail::MeasurementStamp& end) {
        uint64_t elapsed = 0U;
        if (!detail::elapsedUs(begin, end, elapsed)) invalid_.store(true);
        else if (elapsed > std::numeric_limits<uint32_t>::max()) overflow_.store(true);
        else add(instrumentation_overhead_us_, static_cast<uint32_t>(elapsed));
    }
    static void add(std::atomic<uint32_t>& target, uint32_t value) {
        auto current = target.load(std::memory_order_relaxed);
        for (;;) {
            if (std::numeric_limits<uint32_t>::max() - current < value) {
                overflow_.store(true); return;
            }
            if (target.compare_exchange_weak(current, current + value,
                                              std::memory_order_relaxed)) return;
        }
    }
    static void updateMax(std::atomic<uint32_t>& target, uint32_t value) {
        auto current = target.load(std::memory_order_relaxed);
        while (current < value && !target.compare_exchange_weak(current, value,
                                                                std::memory_order_relaxed)) {}
    }
    inline static std::atomic<bool> enabled_{true};
    inline static std::atomic<bool> overflow_{false};
    inline static std::atomic<bool> invalid_{false};
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
#endif
};
using Measurement = SynchronizationMeasurementCollector;
#if DP_OSAL_ENABLE_MEASUREMENT
namespace detail {
struct MeasurementAccess {
    static void recordWait(SynchronizationKind kind, const MeasurementStamp& begin,
                           const MeasurementStamp& end, bool acquired) {
        Measurement::recordWait(kind, begin, end, acquired);
    }
};
} // namespace detail
#endif
} // namespace dp::osal
#endif
