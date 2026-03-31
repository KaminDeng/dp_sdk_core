/** @file interface_queue.h
 *  @brief Abstract typed message-queue interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef IQUEUE_H_
#define IQUEUE_H_

namespace osal {

/** @brief Abstract FIFO message queue with blocking and timed receive.
 *
 *  Provides a type-safe queue for inter-task communication.  Concrete
 *  implementations use a POSIX condition-variable queue or an OS-native
 *  message queue (e.g. FreeRTOS @c xQueueSend / @c xQueueReceive).
 *
 *  @tparam T  Type of the messages stored in the queue. */
template <typename T>
class MessageQueue {
public:
    virtual ~MessageQueue() = default;

    /** @brief Sends a message to the back of the queue (blocks if full).
     *  @param message  Message to enqueue. */
    virtual void send(const T &message) = 0;

    /** @brief Removes and returns the front message, blocking until one is available.
     *  @return The next message from the queue. */
    virtual T receive() = 0;

    /** @brief Attempts to receive a message without blocking.
     *  @param message  Output parameter populated on success.
     *  @return @c true if a message was available, @c false if the queue was empty. */
    virtual bool tryReceive(T &message) = 0;

    /** @brief Attempts to receive a message within a timeout.
     *  @param message  Output parameter populated on success.
     *  @param timeout  Maximum wait time in milliseconds.
     *  @return @c true if a message was received, @c false on timeout. */
    virtual bool receiveFor(T &message, uint32_t timeout) = 0;

    /** @brief Returns the number of messages currently in the queue.
     *  @return Current queue occupancy. */
    [[nodiscard]] virtual size_t size() const = 0;

    /** @brief Discards all messages currently in the queue. */
    virtual void clear() = 0;
};

}  // namespace osal
#endif  // IQUEUE_H_
