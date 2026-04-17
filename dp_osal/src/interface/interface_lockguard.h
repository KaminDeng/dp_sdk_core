/** @file interface_lockguard.h
 *  @brief RAII lock guard template. */
#ifndef DP_OSAL_INTERFACE_LOCKGUARD_H_
#define DP_OSAL_INTERFACE_LOCKGUARD_H_

namespace dp::osal {

/** @brief RAII lock guard for any mutex-like type exposing lock/unlock. */
template <typename MutexType>
class LockGuardBase {
public:
    explicit LockGuardBase(MutexType &mutex) : mutex_(mutex), locked_(mutex_.lock()) {}

    ~LockGuardBase() {
        if (locked_) {
            (void)mutex_.unlock();
        }
    }

    [[nodiscard]] bool isLocked() const { return locked_; }

    LockGuardBase(const LockGuardBase &) = delete;
    LockGuardBase &operator=(const LockGuardBase &) = delete;

private:
    MutexType &mutex_;
    bool locked_;
};

}  // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_LOCKGUARD_H_
