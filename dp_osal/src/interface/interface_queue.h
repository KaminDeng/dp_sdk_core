/**
 * @file interface_queue.h
 * @brief 按消息类型转发 FIFO 操作，并汇总发送/接收的同步等待时间。
 * @details void send 和按值 receive 没有单独错误状态，本层按返回一次调用记录 acquired=true；
 * 这不是对底层传输成功的额外确认。使用 bool 变体才能观察后端返回的失败。
 * 队列容量、消息复制要求和资源生命周期由具体后端实现。
 */
#ifndef DP_OSAL_INTERFACE_QUEUE_H_
#define DP_OSAL_INTERFACE_QUEUE_H_

#include <cstddef>
#include <cstdint>

#include "dp_osal_measurement.h"

namespace dp::osal {

/** @brief 类型化 FIFO 的静态接口。
 * @tparam Impl 具体队列后端。
 * @tparam T 消息类型；须满足所选后端的存储与复制要求。 */
template <typename Impl, typename T>
class QueueBase {
public:
    /**
     * @brief 按后端规则发送一条消息，并将返回的调用计入队列测量。
     * @param[in] message 待复制的消息，调用期间保持有效。
     * @note void 返回无法表示底层错误；当前 CMSIS 可等待队列空间，具体阻塞策略看后端。
     */
    void send(const T &message) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            impl().doSend(message);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), true);
            return;
        }
#endif
        impl().doSend(message);
    }
    /** @brief 尝试发送（timeout=0），队列满时立即返回 false。 */
    /**
     * @param[in] message 待发送消息。
     * @return 后端成功接收消息时为 true；失败不会由本层自动重试。
     */
    bool trySend(const T &message) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTrySend(message);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTrySend(message);
    }
    /**
     * @brief 按后端规则等待接收并按值返回一条消息。
     * @return 后端提供的消息对象。
     * @note 本层没有独立失败状态，不应仅凭一次测量 acquired=true 推断硬件通信成功。
     */
    T receive() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            T message = impl().doReceive();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), true);
            return message;
        }
#endif
        return impl().doReceive();
    }
    /**
     * @brief 不请求等待地接收一条消息，并记录调用时长及结果。
     * @param[out] message 成功时接收消息；失败时不要当作新消息使用。
     * @return 后端报告本次接收成功时为 true。
     */
    bool tryReceive(T &message) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryReceive(message);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTryReceive(message);
    }
    /**
     * @brief 使用相对超时等待接收一条消息。
     * @param[out] message 成功时的消息输出。
     * @param timeout POSIX 当前为毫秒，CMSIS 当前为原生内核 tick，本层不统一换算。
     * @return 成功接收为 true，超时或其他后端失败为 false。
     */
    bool receiveFor(T &message, uint32_t timeout) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doReceiveFor(message, timeout);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doReceiveFor(message, timeout);
    }
    /**
     * @brief 查询后端当前消息数，不预留消息或锁定后续操作。
     * @return 查询瞬间的队列长度，并发操作可立即使结果过时。
     */
    [[nodiscard]] size_t size() const { return impl().doSize(); }
    /**
     * @brief 请求后端丢弃队列中消息。
     * @note 后端清理与并发发送之间的关系由实现决定，不提供额外事务边界。
     */
    void clear() { impl().doClear(); }

protected:
    /**
     * @brief 受保护的非虚析构，由具体队列后端释放资源。
     */
    ~QueueBase() = default;

private:
    /**
     * @brief 获取具体后端的可写引用。
     * @return 当前 Impl 对象。
     */
    Impl &impl() { return *static_cast<Impl *>(this); }
    /**
     * @brief 获取具体后端的只读引用。
     * @return 当前 Impl 的 const 引用。
     */
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_QUEUE_H_
