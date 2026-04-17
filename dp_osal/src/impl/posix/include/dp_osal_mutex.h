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
        clock_gettime(CLOCK_REALTIME, &ts);
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

    int timed_mutex_lock(pthread_mutex_t *mutex, const struct timespec *timeout) {
        struct timespec now;
        while (clock_gettime(CLOCK_REALTIME, &now) == 0) {
            if (now.tv_sec > timeout->tv_sec || (now.tv_sec == timeout->tv_sec && now.tv_nsec >= timeout->tv_nsec)) {
                return ETIMEDOUT;
            }
            int ret = pthread_mutex_trylock(mutex);
            if (ret == 0) {
                return 0;
            }
            if (ret != EBUSY) {
                return ret;
            }
            struct timespec wait_time = {0, 1000000};
            nanosleep(&wait_time, nullptr);
        }
        return errno;
    }

    pthread_mutex_t mutex_;
};

} // namespace dp::osal

#endif  // DP_OSAL_POSIX_MUTEX_H_
