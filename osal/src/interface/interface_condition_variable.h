/** @file interface_condition_variable.h
 *  @brief Abstract interface for OSAL condition variables. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ICONDITION_VARIABLE_H_
#define ICONDITION_VARIABLE_H_

#if OSAL_ENABLE_CONDITION_VAR

#include "osal_mutex.h"

namespace osal {

/** @brief Abstract condition-variable interface.
 *
 *  Provides portable wait/notify primitives that coordinate threads sharing
 *  a mutex.  Implementations map to @c pthread_cond_t (POSIX) or the
 *  equivalent CMSIS-OS2 event/semaphore mechanism. */
class IConditionVariable {
public:
    virtual ~IConditionVariable() = default;

    /** @brief Atomically releases @p mutex and blocks until notified.
     *  @param mutex  The mutex to release while waiting; reacquired on wake. */
    virtual void wait(OSALMutex &mutex) = 0;

    /** @brief Atomically releases @p mutex and blocks until notified or timed out.
     *  @param mutex    The mutex to release while waiting; reacquired on wake.
     *  @param timeout  Maximum wait time in milliseconds.
     *  @return @c true if notified before the timeout, @c false on timeout. */
    virtual bool waitFor(OSALMutex &mutex, uint32_t timeout) = 0;

    /** @brief Wakes one thread waiting on this condition variable. */
    virtual void notifyOne() = 0;

    /** @brief Wakes all threads waiting on this condition variable. */
    virtual void notifyAll() = 0;

    /** @brief Returns the number of threads currently waiting.
     *  @return Current waiter count. */
    [[nodiscard]] virtual int getWaitCount() const = 0;
};

}  // namespace osal

#endif /* OSAL_ENABLE_CONDITION_VAR */

#endif  // ICONDITION_VARIABLE_H_
