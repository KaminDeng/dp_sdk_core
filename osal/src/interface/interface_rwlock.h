/** @file interface_rwlock.h
 *  @brief Abstract reader-writer lock interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef IREAD_WRITE_LOCK_H_
#define IREAD_WRITE_LOCK_H_

namespace osal {

/** @brief Abstract reader-writer lock interface.
 *
 *  Allows multiple concurrent readers or a single exclusive writer.
 *  Implementations may use @c pthread_rwlock_t (POSIX) or a
 *  semaphore-based approach on CMSIS-OS2 targets. */
class IRWLock {
public:
    virtual ~IRWLock() = default;

    /** @brief Acquires the lock for shared reading (blocks until available). */
    virtual void readLock() = 0;

    /** @brief Attempts to acquire the read lock without blocking.
     *  @return @c true if the lock was acquired, @c false otherwise. */
    virtual bool tryReadLock() = 0;

    /** @brief Attempts to acquire the read lock within a timeout.
     *  @param timeout  Maximum wait time in milliseconds.
     *  @return @c true if acquired, @c false on timeout. */
    virtual bool readLockFor(uint32_t timeout) = 0;

    /** @brief Releases a previously acquired read lock. */
    virtual void readUnlock() = 0;

    /** @brief Acquires the lock for exclusive writing (blocks until available). */
    virtual void writeLock() = 0;

    /** @brief Attempts to acquire the write lock without blocking.
     *  @return @c true if the lock was acquired, @c false otherwise. */
    virtual bool tryWriteLock() = 0;

    /** @brief Attempts to acquire the write lock within a timeout.
     *  @param timeout  Maximum wait time in milliseconds.
     *  @return @c true if acquired, @c false on timeout. */
    virtual bool writeLockFor(uint32_t timeout) = 0;

    /** @brief Releases a previously acquired write lock. */
    virtual void writeUnlock() = 0;

    /** @brief Returns the number of active read-lock holders.
     *  @return Current reader count. */
    [[nodiscard]] virtual size_t getReadLockCount() const = 0;

    /** @brief Checks whether the write lock is currently held.
     *  @return @c true if the write lock is held, @c false otherwise. */
    [[nodiscard]] virtual bool isWriteLocked() const = 0;
};

}  // namespace osal
#endif  // IREAD_WRITE_LOCK_H_
