/**
 * @file interface_semaphore.h
 * @brief 计数信号量 CRTP 接口及等待/唤醒统计入口。
 * @details wait/signal 的 void 返回不暴露后端错误，测量记录不能替代业务成功确认。
 * release/waitForMs/open/close 提供受检结果，原有便捷调用仍可丢弃错误。
 * signalFromIsr 的真实 ISR 可用性由后端保证；Linux使用sem_post及无锁计数，
 * 其它POSIX保留mutex/CV且不支持真实ISR/异步signal，
 * 但measurement路径仍须满足来源与原子操作的上下文要求。
 */
#ifndef DP_OSAL_INTERFACE_SEMAPHORE_H_
#define DP_OSAL_INTERFACE_SEMAPHORE_H_

#include <cstdint>

#include "dp_osal_measurement.h"

namespace dp::osal {

enum class WaitResult : uint8_t { kAcquired, kTimeout, kError };

/** @brief 将信号量操作静态绑定到后端。
 * @tparam Impl 提供 doWait/doSignal 等实现的具体类型。 */
template <typename Impl>
class SemaphoreBase {
public:
    /** @brief 显式初始化；max=0使用后端声明上限，重复open失败。 */
    bool open(uint32_t initial = 0U, uint32_t maximum = 0U) noexcept { return impl().doOpen(initial, maximum); }
    /** @brief 无并发使用者时释放；关闭后不能再发送/等待。 */
    bool close() noexcept { return impl().doClose(); }
    /** @brief checked发送，满计数/未初始化/后端错误均失败。 */
    bool release() noexcept {
        const bool result = impl().doRelease();
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) Measurement::record_signal(false);
#endif
        return result;
    }
    /** @brief 同一单调期限内等待，毫秒单位；超时与后端错误分开。 */
    WaitResult waitForMs(uint32_t milliseconds) noexcept {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const auto result = impl().doWaitForMs(milliseconds);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kWake, started, detail::measurementNow(),
                                                  result == WaitResult::kAcquired);
            return result;
        }
#endif
        return impl().doWaitForMs(milliseconds);
    }
    uint32_t maxCount() const noexcept { return impl().doMaxCount(); }

    /**
     * @brief 等待并由后端尝试消费一个令牌，返回后记录唤醒类调用时长。
     * @note 本 void 包装向统计层传 true，即使后端仅打印了错误也无法由此判断成功。
     */
    void wait() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            impl().doWait();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kWake, started, detail::measurementNow(), true);
            return;
        }
#endif
        impl().doWait();
    }
    /**
     * @brief 在线程上下文请求增加令牌并记录一次发送调用。
     * @note 统计的是调用次数，不是已唤醒线程数；满计数或底层失败不经 void 返回值报告。
     */
    void signal() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            impl().doSignal();
            Measurement::record_signal(false);  // 记录前再次检查开关，允许在途关闭后丢弃。
            return;
        }
#endif
        impl().doSignal();
    }
    /**
     * @brief 调用后端的 ISR 发送入口；仅线程上下文标签适配允许记录测量计数。
     * @pre 必须确认所选后端及测量时钟/原子操作支持当前中断上下文。
     */
    void signalFromIsr() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if constexpr (detail::measurementThreadIsrTag()) {
            if (Measurement::enabled()) {
                impl().doSignalFromIsr();
                Measurement::record_signal(true);
                return;
            }
        }
#endif
        impl().doSignalFromIsr();
    }
    /**
     * @brief 尝试消费一个令牌，不请求阻塞等待。
     * @return 后端成功取得令牌时为 true。
     */
    bool tryWait() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryWait();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kWake, started, detail::measurementNow(),
                                                  result);
            return result;
        }
#endif
        return impl().doTryWait();
    }
    /**
     * @brief 按后端相对超时尝试消费令牌，并记录调用耗时。
     * @param timestamp 名称虽为 timestamp，实际是等待量：POSIX 毫秒，CMSIS 原生 tick。
     * @return 成功取得令牌为 true；超时或后端错误为 false。
     */
    bool tryWaitFor(uint32_t timestamp) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryWaitFor(timestamp);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kWake, started, detail::measurementNow(),
                                                  result);
            return result;
        }
#endif
        return impl().doTryWaitFor(timestamp);
    }
    /**
     * @brief 查询后端当前可用令牌数。
     * @return 当前计数的瞬时观测，不预留令牌或保证下次 wait 不阻塞。
     */
    [[nodiscard]] int getValue() const { return impl().doGetValue(); }
    /**
     * @brief 在无并发等待/使用的准备阶段重设可用计数。
     * @param initialValue 非负目标初值，调用者须遵守后端最大计数范围。
     * @note CMSIS 通过排空/释放现有信号量实现，超过上限可只打印错误，不返回实际计数。
     */
    void init(int initialValue) { impl().doInit(initialValue); }

protected:
    /**
     * @brief 受保护的非虚析构，后端管理底层信号量生命周期。
     */
    ~SemaphoreBase() = default;

private:
    /**
     * @brief 静态回转到可写后端。
     * @return 当前 Impl 引用。
     */
    Impl &impl() { return *static_cast<Impl *>(this); }
    /**
     * @brief 静态回转到只读后端。
     * @return 当前 Impl 的 const 引用。
     */
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_SEMAPHORE_H_
