/** @file dp_osal_semaphore.h
 * @brief POSIX计数信号量；Linux显式单调期限，发送不取得用户mutex。
 * 默认构造保持既有正式API零初值自动初始化；DeferredInit构造不取得资源。
 * close/init必须与全部使用者协调，不允许销毁仍被等待的资源。 */
#pragma once
#include "interface_semaphore.h"
#include <atomic>
#include <cerrno>
#include <climits>
#include <ctime>
#include <limits>
#if defined(__linux__)
#include <semaphore.h>
#else
#include <pthread.h>
#endif
namespace dp::osal {
#if defined(__linux__)
class Semaphore : public SemaphoreBase<Semaphore> {
    friend class SemaphoreBase<Semaphore>;

public:
    struct DeferredInit {};
    Semaphore() noexcept { (void)doOpen(0U, 0U); }
    explicit Semaphore(DeferredInit) noexcept {}
    ~Semaphore() { (void)doClose(); }
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

private:
    bool doOpen(uint32_t initial, uint32_t maximum) noexcept {
        if (opened_) return false;
        const uint32_t limit = static_cast<uint32_t>(SEM_VALUE_MAX);
        if (maximum == 0U) maximum = limit;
        if (maximum > limit || initial > maximum) return false;
        if (::sem_init(&semaphore_, 0, initial) != 0) return false;
        maximum_ = maximum;
        credits_.store(initial, std::memory_order_relaxed);
        opened_ = true;
        return true;
    }
    bool doClose() noexcept {
        if (!opened_) return true;
        opened_ = false;
        return ::sem_destroy(&semaphore_) == 0;
    }
    bool doRelease() noexcept {
        if (!opened_) return false;
        uint32_t count = credits_.load(std::memory_order_relaxed);
        do {
            if (count >= maximum_) {
                errno = EOVERFLOW;
                return false;
            }
        } while (!credits_.compare_exchange_weak(count, count + 1U, std::memory_order_relaxed));
        if (::sem_post(&semaphore_) == 0) return true;
        const int error = errno;
        credits_.fetch_sub(1U, std::memory_order_relaxed);
        errno = error;
        return false;
    }
    WaitResult consume(int result) noexcept {
        if (result == 0) {
            credits_.fetch_sub(1U, std::memory_order_relaxed);
            return WaitResult::kAcquired;
        }
        return errno == ETIMEDOUT || errno == EAGAIN ? WaitResult::kTimeout : WaitResult::kError;
    }
    WaitResult doWaitForMs(uint32_t milliseconds) noexcept {
        if (!opened_) return WaitResult::kError;
        timespec deadline{};
        if (::clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return WaitResult::kError;
        if (deadline.tv_sec > std::numeric_limits<time_t>::max() - static_cast<time_t>(milliseconds / 1000U) - 1)
            return WaitResult::kError;
        deadline.tv_sec += static_cast<time_t>(milliseconds / 1000U);
        deadline.tv_nsec += static_cast<long>(milliseconds % 1000U) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }
        for (;;) {
            const int result = ::sem_clockwait(&semaphore_, CLOCK_MONOTONIC, &deadline);
            if (result == 0 || errno != EINTR) return consume(result);
            timespec now{};
            if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) return WaitResult::kError;
            if (now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
                return WaitResult::kTimeout;
        }
    }
    void doWait() noexcept {
        if (!opened_) return;
        int result;
        do {
            result = ::sem_wait(&semaphore_);
        } while (result != 0 && errno == EINTR);
        (void)consume(result);
    }
    void doSignal() noexcept { (void)doRelease(); }
    void doSignalFromIsr() noexcept { (void)doRelease(); }
    bool doTryWait() noexcept { return opened_ && consume(::sem_trywait(&semaphore_)) == WaitResult::kAcquired; }
    bool doTryWaitFor(uint32_t milliseconds) noexcept { return doWaitForMs(milliseconds) == WaitResult::kAcquired; }
    int doGetValue() const noexcept { return opened_ ? static_cast<int>(credits_.load(std::memory_order_relaxed)) : 0; }
    uint32_t doMaxCount() const noexcept { return maximum_; }
    void doInit(int initial) noexcept {
        if (initial < 0 || static_cast<uint32_t>(initial) > maximum_) return;
        if (!opened_) {
            (void)doOpen(static_cast<uint32_t>(initial), maximum_);
            return;
        }
        for (;;) {
            if (::sem_trywait(&semaphore_) == 0) continue;
            if (errno == EINTR) continue;
            if (errno != EAGAIN) return;
            break;
        }
        credits_.store(0U, std::memory_order_relaxed);
        for (int i = 0; i < initial; ++i)
            if (!doRelease()) break;
    }
    sem_t semaphore_{};
    static_assert(std::atomic<uint32_t>::is_always_lock_free, "计数发送要求无锁32位原子");
    std::atomic<uint32_t> credits_{0U};
    uint32_t maximum_{static_cast<uint32_t>(SEM_VALUE_MAX)};
    bool opened_{};
};
#else
/** @brief 非Linux POSIX使用原生mutex/CV；checked初始化不依赖C++异常或堆。 */
class Semaphore : public SemaphoreBase<Semaphore> {
    friend class SemaphoreBase<Semaphore>;

public:
    struct DeferredInit {};
    Semaphore() noexcept { (void)doOpen(0U, 0U); }
    explicit Semaphore(DeferredInit) noexcept {}
    ~Semaphore() { (void)doClose(); }
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

private:
    bool doOpen(uint32_t initial, uint32_t maximum) noexcept {
        if (opened_) return false;
        if (maximum == 0U) maximum = INT32_MAX;
        if (maximum > INT32_MAX || initial > maximum) return false;
        if (::pthread_mutex_init(&mutex_, nullptr) != 0) return false;
        pthread_condattr_t attr{};
        if (::pthread_condattr_init(&attr) != 0) {
            (void)::pthread_mutex_destroy(&mutex_);
            return false;
        }
#if defined(__APPLE__)
        const int configured = 0;  // Darwin用原生相对单调等待入口，不使用墙钟绝对期限。
#else
        const int configured = ::pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif
        const int created = configured == 0 ? ::pthread_cond_init(&condition_, &attr) : configured;
        const int released = ::pthread_condattr_destroy(&attr);
        if (created != 0 || released != 0) {
            if (created == 0) (void)::pthread_cond_destroy(&condition_);
            (void)::pthread_mutex_destroy(&mutex_);
            return false;
        }
        count_ = initial;
        maximum_ = maximum;
        opened_ = true;
        return true;
    }
    bool doClose() noexcept {
        if (!opened_) return true;
        opened_ = false;
        const int condition = ::pthread_cond_destroy(&condition_);
        const int mutex = ::pthread_mutex_destroy(&mutex_);
        return condition == 0 && mutex == 0;
    }
    bool doRelease() noexcept {
        if (!opened_ || ::pthread_mutex_lock(&mutex_) != 0) return false;
        bool good = count_ < maximum_;
        if (good) {
            ++count_;
            if (::pthread_cond_signal(&condition_) != 0) {
                --count_;
                good = false;
            }
        }
        return ::pthread_mutex_unlock(&mutex_) == 0 && good;
    }
    WaitResult doWaitForMs(uint32_t milliseconds) noexcept {
        if (!opened_) return WaitResult::kError;
        timespec deadline{};
        if (::clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return WaitResult::kError;
        if (deadline.tv_sec > std::numeric_limits<time_t>::max() - static_cast<time_t>(milliseconds / 1000U) - 1)
            return WaitResult::kError;
        deadline.tv_sec += static_cast<time_t>(milliseconds / 1000U);
        deadline.tv_nsec += static_cast<long>(milliseconds % 1000U) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }
        if (::pthread_mutex_lock(&mutex_) != 0) return WaitResult::kError;
        const auto finish = [this](WaitResult result) noexcept {
            return ::pthread_mutex_unlock(&mutex_) == 0 ? result : WaitResult::kError;
        };
        while (count_ == 0U) {
#if defined(__APPLE__)
            timespec now{};
            if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) return finish(WaitResult::kError);
            if (now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
                return finish(WaitResult::kTimeout);
            timespec remaining{deadline.tv_sec - now.tv_sec, deadline.tv_nsec - now.tv_nsec};
            if (remaining.tv_nsec < 0) {
                --remaining.tv_sec;
                remaining.tv_nsec += 1000000000L;
            }
            const int result = ::pthread_cond_timedwait_relative_np(&condition_, &mutex_, &remaining);
#else
            const int result = ::pthread_cond_timedwait(&condition_, &mutex_, &deadline);
#endif
            if (result == ETIMEDOUT && count_ == 0U) return finish(WaitResult::kTimeout);
            if (result != 0 && result != EINTR && result != ETIMEDOUT) return finish(WaitResult::kError);
        }
        --count_;
        return finish(WaitResult::kAcquired);
    }
    void doWait() noexcept {
        if (!opened_ || ::pthread_mutex_lock(&mutex_) != 0) return;
        int result{};
        while (count_ == 0U && result == 0) result = ::pthread_cond_wait(&condition_, &mutex_);
        if (result == 0) --count_;
        (void)::pthread_mutex_unlock(&mutex_);
    }
    void doSignal() noexcept { (void)doRelease(); }
    void doSignalFromIsr() noexcept { (void)doRelease(); }  // 仅线程模拟标签，不支持真实ISR/异步signal。
    bool doTryWait() noexcept {
        if (!opened_ || ::pthread_mutex_lock(&mutex_) != 0) return false;
        const bool acquired = count_ != 0U;
        if (acquired) --count_;
        return ::pthread_mutex_unlock(&mutex_) == 0 && acquired;
    }
    bool doTryWaitFor(uint32_t milliseconds) noexcept { return doWaitForMs(milliseconds) == WaitResult::kAcquired; }
    int doGetValue() const noexcept {
        if (!opened_ || ::pthread_mutex_lock(&mutex_) != 0) return 0;
        const int count = static_cast<int>(count_);
        (void)::pthread_mutex_unlock(&mutex_);
        return count;
    }
    uint32_t doMaxCount() const noexcept { return maximum_; }
    void doInit(int initial) noexcept {
        if (initial < 0 || static_cast<uint32_t>(initial) > maximum_) return;
        if (!opened_) {
            (void)doOpen(static_cast<uint32_t>(initial), maximum_);
            return;
        }
        if (::pthread_mutex_lock(&mutex_) != 0) return;
        count_ = static_cast<uint32_t>(initial);
        (void)::pthread_mutex_unlock(&mutex_);
    }
    mutable pthread_mutex_t mutex_{};
    pthread_cond_t condition_{};
    uint32_t count_{};
    uint32_t maximum_{INT32_MAX};
    bool opened_{};
};

#endif

}  // namespace dp::osal
