/** @file interface_lockguard.h
 *  @brief Abstract RAII lock-guard interface. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ILOCKGUARD_H_
#define ILOCKGUARD_H_

namespace osal {

/** @brief RAII-style scoped lock manager interface.
 *
 *  Concrete implementations acquire a lock on construction and release it
 *  on destruction, preventing lock leaks.  Use @c isLocked() to detect
 *  whether the underlying lock was actually acquired (e.g. after a
 *  tryLock-style guard). */
class ILockGuard {
public:
    virtual ~ILockGuard() = default;

    /** @brief Checks whether the guarded lock is currently held.
     *  @return @c true if the lock is held, @c false otherwise. */
    virtual bool isLocked() = 0;
};
}  // namespace osal

#endif  // ILOCKGUARD_H_
