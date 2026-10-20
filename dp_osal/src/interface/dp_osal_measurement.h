/**
 * @file dp_osal_measurement.h
 * @brief 有界存储的同步耗时汇总：锁、信号量、队列及插桩开销。
 * @details 时间差按32位微秒模运算，调用者应限制观测窗口。统计使用原子字段，
 * 但没有全表锁存，也不提供事件级回放、真实调度延迟分解或运动授权。
 * 累加越界时置overflow并拒绝本次加法，不保证将目标字段饱和写到最大值。
 */
#ifndef DP_OSAL_MEASUREMENT_H_
#define DP_OSAL_MEASUREMENT_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>

#include "dp_osal_thread_snapshot.h"

/**
 * @brief 平台可选的C链接微秒时钟hook，位于全局命名空间便于C端口实现。
 * @return 可回绕的32位单调微秒计数，时钟质量和上下文可用性由端口保证。
 * @note GCC声明为弱符号，缺省时MCU返回0、主机回退steady_clock；非GCC链接行为由工具链保证。
 */
#if defined(__GNUC__)
extern "C" uint32_t dp_osal_port_measurement_now_us(void) __attribute__((weak));
#else
extern "C" uint32_t dp_osal_port_measurement_now_us(void);
#endif

namespace dp::osal {

/**
 * @brief 选择同步事件的统计桶；record_wait中其它枚举值落入队列分支。
 */
enum class SynchronizationKind : uint8_t {
    kLock = 0U,  ///< 锁获取类事件。
    kWake = 1U,  ///< 信号量等待类事件。
    kQueue = 2U, ///< 队列发送/接收类事件。
};

/**
 * @brief 按字段读取的测量快照，时间单位微秒，计数单位次。
 * @note 需要结合valid/overflow/instrumentation_enabled及实际采样边界解释，不能当作事务快照。
 */
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
    uint8_t valid{}; ///< 仅当启用、未溢出且时钟入口可用时为1，不证明样本量/整份一致性。
    uint8_t overflow{}; ///< 某次累加超出uint32_t范围后锁存为1，reset清除。
    uint8_t instrumentation_enabled{}; ///< 快照时开关是否启用；关闭并不清空旧统计。
    uint8_t time_domain{static_cast<uint8_t>(MeasurementTimeDomain::kMonotonicMicroseconds)}; ///< 声明计数以单调微秒为时间域，不证明平台hook质量。
};

/**
 * @brief 用固定数量原子字段汇总OSAL同步耗时，不保存无界事件日志。
 * @details 默认启用；推荐在无并发写入的边界reset，再执行有界负载并取snapshot。
 * 原子字段避免单字段数据竞争，但一次事件涉及多个字段，snapshot/reset均不是整体原子操作。
 * MCU时钟通过弱hook注入，不将具体HAL/RTOS句柄暴露到公共接口。
 */
class SynchronizationMeasurementCollector {
public:
    /**
     * @brief 优先调用平台时钟hook，否则主机回退steady_clock，MCU无hook返回零。
     * @return 可回绕的32位微秒值；本层不验证hook是否真的单调或在运行。
     */
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

    /**
     * @brief 检查时间来源入口是否具备，不读取或校准时钟。
     * @return MCU检查hook非空；主机因有steady_clock回退而始终true。
     */
    static bool clock_available() noexcept {
#if defined(DPSDK_MCU_FIRMWARE)
        return dp_osal_port_measurement_now_us != nullptr;
#else
        return true;
#endif
    }

    /**
     * @brief 发布全局插桩开关，不清空既有计数。
     * @param enabled 是否允许后续记录路径写入统计。
     */
    static void set_enabled(bool enabled) { enabled_.store(enabled, std::memory_order_release); }
    /**
     * @brief 以acquire读取全局开关。
     * @return 当前开关状态，不能据此排除已经进入的并发记录。
     */
    static bool enabled() { return enabled_.load(std::memory_order_acquire); }

    /**
     * @brief 逐字段清空累计量、最大值、溢出标志及样本次数，保留开关设置。
     * @pre 若需要严格窗口边界，应先协调所有记录者停止；逐字段reset不能切断并发事件。
     */
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

    /**
     * @brief 逐字段读取原子存储，附上当前开关、溢出和时钟入口有效性。
     * @return 统计值快照；valid不要求sample_sequence>0，不证明时间源精度或字段同一时刻。
     */
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
        out.valid = (out.instrumentation_enabled != 0U && out.overflow == 0U && clock_available()) ? 1U : 0U;
        return out;
    }

    /**
     * @brief 在启用时累计一次同步调用的次数、总耗时、最大值与部分插桩开销。
     * @param kind 锁、唤醒或队列类别，未识别值也走队列桶。
     * @param started_at_us 调用开始的32位微秒计数。
     * @param finished_at_us 调用结束的同域32位微秒计数。
     * @param acquired 仅锁桶用false累计lock_contended_count，其它桶忽略此成功标记。
     * @note 使用模2^32差，不单独检查半圈/多次回绕；void后端包装传true也不构成底层成功证据。
     */
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

    /**
     * @brief 记录一次信号发送包装调用，ISR标记另外计入子集。
     * @param from_isr 是否来自ISR用途的包装入口。
     * @note 不知道底层释放是否成功，也不判断有多少线程醒来；中断安全取决于端口时钟和原子实现。
     */
    static void record_signal(bool from_isr) {
        if (!enabled()) return;
        const uint32_t overhead_started = now_us();
        add(wake_signal_count_, 1U);
        if (from_isr) add(wake_isr_signal_count_, 1U);
        add(sample_sequence_, 1U);
        add(instrumentation_overhead_us_, runtime_counter_delta_u32(now_us(), overhead_started));
    }

    /**
     * @brief 只累计调用者提供的一段插桩耗时，不增加样本次数。
     * @param started_at_us 开始微秒计数。
     * @param finished_at_us 同域结束微秒计数。
     */
    static void record_instrumentation(uint32_t started_at_us, uint32_t finished_at_us) {
        if (!enabled()) return;
        add(instrumentation_overhead_us_, runtime_counter_delta_u32(finished_at_us, started_at_us));
    }

private:
    /**
     * @brief 用弱CAS累加单个原子uint32_t，检测到范围不足则置overflow并放弃本次加法。
     * @param target 待更新的统计字段。
     * @param value 非负累加量。
     * @note 溢出路径保留target旧值，不是饱和写MAX；并发竞争可导致多次重试，无固定尝试次数上界。
     */
    static void add(std::atomic<uint32_t> &target, uint32_t value) {
        uint32_t current = target.load(std::memory_order_relaxed);
        for (;;) {
            if (std::numeric_limits<uint32_t>::max() - current < value) {
                overflow_.store(true, std::memory_order_release);
                // 仅把本地current设为退出标记，不把target原子写到MAX；旧累计值保留。
                current = std::numeric_limits<uint32_t>::max();
            } else if (target.compare_exchange_weak(current, current + value,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed)) {
                return;
            }
            if (current == std::numeric_limits<uint32_t>::max()) return;
        }
    }

    /**
     * @brief 仅当候选值更大时用弱CAS提升最大值。
     * @param target 保存当前最大值的原子字段。
     * @param value 本次观测值。
     * @note compare_exchange失败会更新本地current，重试到不再需要更新或成功为止。
     */
    static void update_max(std::atomic<uint32_t> &target, uint32_t value) {
        uint32_t current = target.load(std::memory_order_relaxed);
        while (current < value && !target.compare_exchange_weak(current, value,
                                                                std::memory_order_relaxed,
                                                                std::memory_order_relaxed)) {
        }
    }

    inline static std::atomic<bool> enabled_{true}; ///< 全局开关，默认启用，release/acquire发布与读取。
    inline static std::atomic<bool> overflow_{false}; ///< 累加越界锁存；reset才清除，不承担事务快照版本作用。
    inline static std::atomic<uint32_t> lock_wait_count_{0U}; ///< 锁获取包装的记录次数，包含成功与失败。
    inline static std::atomic<uint32_t> lock_contended_count_{0U}; ///< acquired=false的锁调用次数；成功但确实等待过的调用不计入此字段。
    inline static std::atomic<uint32_t> lock_wait_total_us_{0U}; ///< 锁获取包装耗时之和，单位微秒，包含底层整个调用。
    inline static std::atomic<uint32_t> lock_wait_max_us_{0U}; ///< 单次锁获取包装的最大观测耗时，微秒。
    inline static std::atomic<uint32_t> wake_wait_count_{0U}; ///< 信号量等待包装的记录次数，不独立区分成功与失败。
    inline static std::atomic<uint32_t> wake_wait_total_us_{0U}; ///< 等待包装耗时累计，微秒。
    inline static std::atomic<uint32_t> wake_wait_max_us_{0U}; ///< 等待包装最大观测耗时，微秒。
    inline static std::atomic<uint32_t> wake_signal_count_{0U}; ///< 线程与ISR发送包装的总记录次数，不等于成功唤醒线程数。
    inline static std::atomic<uint32_t> wake_isr_signal_count_{0U}; ///< from_isr=true的发送记录次数，是总发送次数的子集。
    inline static std::atomic<uint32_t> queue_wait_count_{0U}; ///< 队列发送/接收包装的记录次数。
    inline static std::atomic<uint32_t> queue_wait_total_us_{0U}; ///< 队列包装耗时累计，微秒。
    inline static std::atomic<uint32_t> queue_wait_max_us_{0U}; ///< 队列包装最大观测耗时，微秒。
    inline static std::atomic<uint32_t> instrumentation_overhead_us_{0U}; ///< 收集器所观测的部分插桩开销累计，微秒，不是全链路精确校准值。
    inline static std::atomic<uint32_t> sample_sequence_{0U}; ///< record_wait/record_signal记录次数，不是保护整份快照的seqlock版本。
};

/**
 * @brief 同步收集器的短别名，供接口包装及测试调用。
 */
using Measurement = SynchronizationMeasurementCollector;

}  // namespace dp::osal

#endif  // DP_OSAL_MEASUREMENT_H_
