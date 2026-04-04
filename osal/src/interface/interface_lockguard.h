/** @file interface_lockguard.h
 *  @brief RAII lock guard template. */
#ifndef OSAL_INTERFACE_LOCKGUARD_H_
#define OSAL_INTERFACE_LOCKGUARD_H_

namespace osal {

/** @brief RAII lock guard for any mutex-like type exposing lock/unlock. */
template <typename MutexType>
class LockGuard {
public:
    explicit LockGuard(MutexType &mutex) : mutex_(mutex), locked_(mutex_.lock()) {}

    ~LockGuard() {
        if (locked_) {
            (void)mutex_.unlock();
        }
    }

    [[nodiscard]] bool isLocked() const { return locked_; }

    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;

private:
    MutexType &mutex_;
    bool locked_;
};

}  // namespace osal

#endif  // OSAL_INTERFACE_LOCKGUARD_H_
