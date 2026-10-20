/**
 * @file interface_semaphore.h
 * @brief 计数信号量 CRTP 接口及等待/唤醒统计入口。
 * @details wait/signal 的 void 返回不暴露后端错误，测量记录不能替代业务成功确认。
 * signalFromIsr 表达调用用途，是否可在真实 ISR 使用必须由后端保证；
 * POSIX 模拟实现仍使用 std::mutex，不具有 MCU ISR/异步信号安全能力。
 */
#ifndef DP_OSAL_INTERFACE_SEMAPHORE_H_
#define DP_OSAL_INTERFACE_SEMAPHORE_H_

#include <cstdint>

#include "dp_osal_measurement.h"

namespace dp::osal {

/** @brief 将信号量操作静态绑定到后端。
 * @tparam Impl 提供 doWait/doSignal 等实现的具体类型。 */
template <typename Impl>
class SemaphoreBase {
public:
    /**
     * @brief 等待并由后端尝试消费一个令牌，返回后记录唤醒类调用时长。
     * @note 本 void 包装向统计层传 true，即使后端仅打印了错误也无法由此判断成功。
     */
    void wait() {
        const uint32_t started = Measurement::now_us();
        impl().doWait();
        Measurement::record_wait(SynchronizationKind::kWake, started, Measurement::now_us(), true);
    }
    /**
     * @brief 在线程上下文请求增加令牌并记录一次发送调用。
     * @note 统计的是调用次数，不是已唤醒线程数；满计数或底层失败不经 void 返回值报告。
     */
    void signal() {
        impl().doSignal();
        Measurement::record_signal(false);
    }
    /**
     * @brief 调用后端的 ISR 发送入口，并同时增加发送及 ISR 发送测量计数。
     * @pre 必须确认所选后端及测量时钟/原子操作支持当前中断上下文。
     */
    void signalFromIsr() {
        impl().doSignalFromIsr();
        Measurement::record_signal(true);
    }
    /**
     * @brief 尝试消费一个令牌，不请求阻塞等待。
     * @return 后端成功取得令牌时为 true。
     */
    bool tryWait() {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doTryWait();
        Measurement::record_wait(SynchronizationKind::kWake, started, Measurement::now_us(), result);
        return result;
    }
    /**
     * @brief 按后端相对超时尝试消费令牌，并记录调用耗时。
     * @param timestamp 名称虽为 timestamp，实际是等待量：POSIX 毫秒，CMSIS 原生 tick。
     * @return 成功取得令牌为 true；超时或后端错误为 false。
     */
    bool tryWaitFor(uint32_t timestamp) {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doTryWaitFor(timestamp);
        Measurement::record_wait(SynchronizationKind::kWake, started, Measurement::now_us(), result);
        return result;
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

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_SEMAPHORE_H_
