/** @file interface_mutex.h
 *  @brief Abstract mutex interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef IMUTEX_H_
#define IMUTEX_H_

namespace osal {

/** @brief Abstract mutual-exclusion lock interface.
 *
 *  Maps to a @c pthread_mutex_t on POSIX or a CMSIS-OS2 mutex on
 *  embedded targets.  All operations are non-recursive by default unless
 *  the concrete backend is configured otherwise. */
class IMutex {
public:
    virtual ~IMutex() = default;

    /** @brief Blocks the caller until the mutex is acquired.
     *  @return @c true on success, @c false on error. */
    virtual bool lock() = 0;

    /** @brief Releases the mutex.
     *  @return @c true on success, @c false on error. */
    virtual bool unlock() = 0;

    /** @brief Attempts to acquire the mutex without blocking.
     *  @return @c true if the mutex was acquired, @c false if already locked. */
    virtual bool tryLock() = 0;

    /** @brief Attempts to acquire the mutex within a timeout.
     *  @param timeout  Maximum wait time in milliseconds.
     *  @return @c true if acquired before timeout, @c false on timeout or error. */
    virtual bool tryLockFor(uint32_t timeout) = 0;
};
}  // namespace osal

#endif  // IMUTEX_H_
