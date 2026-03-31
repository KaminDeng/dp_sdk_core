/** @file interface_spin_lock.h
 *  @brief Abstract spin-lock interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ISPIN_LOCK_H_
#define ISPIN_LOCK_H_

namespace osal {

/** @brief Abstract spin-lock interface.
 *
 *  Provides busy-wait mutual exclusion suitable for short critical sections.
 *  On POSIX the implementation uses @c std::atomic<bool> to avoid calling
 *  OS primitives that may block; on CMSIS-OS2 it wraps an OS mutex. */
class ISpinLock {
public:
    virtual ~ISpinLock() = default;

    /** @brief Spins until the lock is acquired. */
    virtual void lock() = 0;

    /** @brief Attempts to acquire the lock without spinning.
     *  @return @c true if the lock was acquired, @c false if already held. */
    virtual bool tryLock() = 0;

    /** @brief Attempts to acquire the lock within a timeout.
     *  @param timeout  Maximum wait time in milliseconds.
     *  @return @c true if acquired, @c false on timeout. */
    virtual bool lockFor(uint32_t timeout) = 0;

    /** @brief Releases the spin lock. */
    virtual void unlock() = 0;

    /** @brief Checks whether the spin lock is currently held.
     *  @return @c true if locked, @c false otherwise. */
    [[nodiscard]] virtual bool isLocked() const = 0;
};

}  // namespace osal
#endif  // ISPIN_LOCK_H_
