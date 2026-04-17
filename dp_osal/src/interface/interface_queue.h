/** @file interface_queue.h
 *  @brief CRTP typed message-queue interface for OSAL. */
#ifndef OSAL_INTERFACE_QUEUE_H_
#define OSAL_INTERFACE_QUEUE_H_

#include <cstddef>
#include <cstdint>

namespace osal {

/** @brief CRTP base for FIFO message queues.
 *  @tparam Impl Concrete implementation type.
 *  @tparam T Message type. */
template <typename Impl, typename T>
class QueueBase {
public:
    void send(const T &message) { impl().doSend(message); }
    /** @brief 尝试发送（timeout=0），队列满时立即返回 false。 */
    bool trySend(const T &message) { return impl().doTrySend(message); }
    T receive() { return impl().doReceive(); }
    bool tryReceive(T &message) { return impl().doTryReceive(message); }
    bool receiveFor(T &message, uint32_t timeout) { return impl().doReceiveFor(message, timeout); }
    [[nodiscard]] size_t size() const { return impl().doSize(); }
    void clear() { impl().doClear(); }

protected:
    ~QueueBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace osal

#endif  // OSAL_INTERFACE_QUEUE_H_
