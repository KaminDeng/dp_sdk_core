//
// Created by kamin.deng on 2024/8/22.
//
#ifndef __OSAL_LOCK_GUARD_H__
#define __OSAL_LOCK_GUARD_H__

#include "interface_lockguard.h"
#include "osal_debug.h"
#include "osal_mutex.h"

namespace osal {

class OSALLockGuard : public ILockGuard {
public:
    explicit OSALLockGuard(OSALMutex &mutex) : mutex_(mutex), locked_(false) {
        locked_ = mutex_.lock();
        if (locked_) {
            OSAL_LOGD("Mutex locked successfully in OSALLockGuard\n");
        } else {
            OSAL_LOGE("Failed to lock mutex in OSALLockGuard\n");
        }
    }

    ~OSALLockGuard() override {
        if (locked_) {
            if (mutex_.unlock()) {
                OSAL_LOGD("Mutex unlocked successfully in OSALLockGuard\n");
            } else {
                OSAL_LOGE("Failed to unlock mutex in OSALLockGuard\n");
            }
        }
    }

    [[nodiscard]] bool isLocked() override { return locked_; }

private:
    OSALMutex &mutex_;
    bool locked_;
};

}  // namespace osal

#endif  // __OSAL_LOCK_GUARD_H__