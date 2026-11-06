//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_POSIX_MUTEX_H_
#define DP_OSAL_POSIX_MUTEX_H_

#include <pthread.h>

#include <cerrno>
#include <ctime>

#include "interface_mutex.h"
#include "dp_osal_debug.h"

namespace dp::osal {

class Mutex : public MutexBase<Mutex> {
    friend class MutexBase<Mutex>;

public:
    Mutex() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mutex_, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    ~Mutex() { pthread_mutex_destroy(&mutex_); }

    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    pthread_mutex_t &getNativeHandle() { return mutex_; }

private:
    bool doLock() {
        int ret = pthread_mutex_lock(&mutex_);
        if (ret != 0) {
            DP_OSAL_LOGE("Failed to lock mutex, return code %d\n", ret);
            return false;
        }
        DP_OSAL_LOGD("Mutex locked successfully\n");
        return true;
    }

    bool doUnlock() {
        int ret = pthread_mutex_unlock(&mutex_);
        if (ret != 0) {
            DP_OSAL_LOGE("Failed to unlock mutex, return code %d\n", ret);
            return false;
        }
        DP_OSAL_LOGD("Mutex unlocked successfully\n");
        return true;
    }

    bool doTryLock() {
        int ret = pthread_mutex_trylock(&mutex_);
        if (ret != 0) {
            DP_OSAL_LOGD("Failed to try lock mutex, return code %d\n", ret);
            return false;
        }
        DP_OSAL_LOGD("Mutex try lock successfully\n");
        return true;
    }

    bool doTryLockFor(uint32_t timeout) {
        struct timespec ts;
        if (timeout == 0U) return doTryLock();
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return false;
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (timeout % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }

        int ret = timed_mutex_lock(&mutex_, &ts);
        if (ret != 0) {
            DP_OSAL_LOGD("Failed to timed lock mutex, return code %d\n", ret);
            return false;
        }
        DP_OSAL_LOGD("Mutex timed lock successfully\n");
        return true;
    }

    /** @brief 新接口复用原毫秒实现；旧整数接口形态和单位均不改变。 */
    bool doTryLockForMs(uint32_t timeout_ms) { return doTryLockFor(timeout_ms); }

    /** @brief 单调期限下有限轮询；每次休眠不超过剩余值，EINTR后重读原期限。
     * @details 调度恢复可能晚于期限；恢复时锁已可用允许成功，符合定时获取语义，不保证WCET。 */
    int timed_mutex_lock(pthread_mutex_t *mutex, const struct timespec *timeout) {
        for (;;) {
            const int ret = pthread_mutex_trylock(mutex);
            if (ret != EBUSY) return ret;
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return errno;
            if (now.tv_sec > timeout->tv_sec || (now.tv_sec == timeout->tv_sec && now.tv_nsec >= timeout->tv_nsec))
                return ETIMEDOUT;
            time_t seconds = timeout->tv_sec - now.tv_sec;
            long nanoseconds = timeout->tv_nsec - now.tv_nsec;
            if (nanoseconds < 0) {
                --seconds;
                nanoseconds += 1000000000L;
            }
            const long pause_ns = seconds > 0 || nanoseconds > 1000000L ? 1000000L : nanoseconds;
            const struct timespec pause {
                0, pause_ns
            };
            if (nanosleep(&pause, nullptr) != 0 && errno != EINTR) return errno;
        }
    }

    pthread_mutex_t mutex_;
};

}  // namespace dp::osal

#endif  // DP_OSAL_POSIX_MUTEX_H_
