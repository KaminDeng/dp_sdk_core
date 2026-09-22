/** @file interface_queue.h
 *  @brief CRTP typed message-queue interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_QUEUE_H_
#define DP_OSAL_INTERFACE_QUEUE_H_

#include <cstddef>
#include <cstdint>

#include "dp_osal_measurement.h"

namespace dp::osal {

/** @brief CRTP base for FIFO message queues.
 *  @tparam Impl Concrete implementation type.
 *  @tparam T Message type. */
template <typename Impl, typename T>
class QueueBase {
public:
    void send(const T &message) {
        const uint32_t started = Measurement::now_us();
        impl().doSend(message);
        Measurement::record_wait(SynchronizationKind::kQueue, started, Measurement::now_us(), true);
    }
    /** @brief 尝试发送（timeout=0），队列满时立即返回 false。 */
    bool trySend(const T &message) {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doTrySend(message);
        Measurement::record_wait(SynchronizationKind::kQueue, started, Measurement::now_us(), result);
        return result;
    }
    T receive() {
        const uint32_t started = Measurement::now_us();
        T message = impl().doReceive();
        Measurement::record_wait(SynchronizationKind::kQueue, started, Measurement::now_us(), true);
        return message;
    }
    bool tryReceive(T &message) {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doTryReceive(message);
        Measurement::record_wait(SynchronizationKind::kQueue, started, Measurement::now_us(), result);
        return result;
    }
    bool receiveFor(T &message, uint32_t timeout) {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doReceiveFor(message, timeout);
        Measurement::record_wait(SynchronizationKind::kQueue, started, Measurement::now_us(), result);
        return result;
    }
    [[nodiscard]] size_t size() const { return impl().doSize(); }
    void clear() { impl().doClear(); }

protected:
    ~QueueBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_QUEUE_H_
