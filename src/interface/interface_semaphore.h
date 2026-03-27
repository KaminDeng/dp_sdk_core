/** @file interface_semaphore.h
 *  @brief Abstract counting-semaphore interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ISEMAPHORE_H_
#define ISEMAPHORE_H_

#include <stdint.h>

namespace osal {

/** @brief Abstract counting semaphore interface.
 *
 *  Provides portable wait/signal primitives backed by @c pthread_cond_t
 *  (POSIX) or a CMSIS-OS2 semaphore on embedded targets. */
class ISemaphore {
public:
    virtual ~ISemaphore() = default;

    /** @brief Decrements the semaphore count, blocking until non-zero. */
    virtual void wait() = 0;

    /** @brief Increments the semaphore count, waking one waiting thread. */
    virtual void signal() = 0;

    /** @brief Attempts to decrement the semaphore without blocking.
     *  @return @c true if the semaphore was decremented, @c false if zero. */
    virtual bool tryWait() = 0;

    /** @brief Attempts to decrement the semaphore within a timeout.
     *  @param timestamp  Maximum wait time in milliseconds.
     *  @return @c true if decremented before timeout, @c false on timeout. */
    virtual bool tryWaitFor(uint32_t timestamp) = 0;

    /** @brief Returns the current semaphore count.
     *  @return Current count value. */
    [[nodiscard]] virtual int getValue() const = 0;

    /** @brief Resets the semaphore to a given initial count.
     *  @param initialValue  New initial count value. */
    virtual void init(int initialValue) = 0;
};

}  // namespace osal
#endif  // ISEMAPHORE_H_
