#pragma once
#if !DP_OSAL_ENABLE_LEGACY_COMPAT
#error "此旧接口仅由dp_osal_legacy_compat目标显式装配"
#endif

#if !defined(__linux__)
#error "当前Host I/O后端要求Linux；其他平台必须提供显式实现"
#endif
#include "interface_host_wake.h"
#include <cerrno>
#include <ctime>
#include <semaphore.h>
namespace dp::osal {
class OSALHostWake : public HostWakeBase<OSALHostWake> {
    friend class HostWakeBase<OSALHostWake>;
public:
    OSALHostWake() noexcept = default;
    ~OSALHostWake() { (void)doClose(); }
    OSALHostWake(const OSALHostWake&) = delete;
    OSALHostWake& operator=(const OSALHostWake&) = delete;
private:
    bool doOpen() noexcept {
        if (opened_) return false;
        opened_ = ::sem_init(&semaphore_, 0, 0U) == 0;
        return opened_;
    }
    bool doSignal() noexcept { return opened_ && ::sem_post(&semaphore_) == 0; }
    HostWakeResult doWaitForMs(uint32_t milliseconds) noexcept {
        if (!opened_) return HostWakeResult::kError;
        timespec deadline{};
        if (::clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return HostWakeResult::kError;
        deadline.tv_sec += static_cast<time_t>(milliseconds / 1000U);
        deadline.tv_nsec += static_cast<long>(milliseconds % 1000U) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) { ++deadline.tv_sec; deadline.tv_nsec -= 1000000000L; }
        for (;;) {
            const int result = ::sem_clockwait(&semaphore_, CLOCK_MONOTONIC, &deadline);
            if (result == 0) return HostWakeResult::kSignaled;
            if (errno == ETIMEDOUT) return HostWakeResult::kTimeout;
            if (errno != EINTR) return HostWakeResult::kError;
            timespec now{};
            if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) return HostWakeResult::kError;
            if (now.tv_sec > deadline.tv_sec ||
                (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) return HostWakeResult::kTimeout;
        }
    }
    bool doClose() noexcept {
        if (!opened_) return true;
        opened_ = false;
        return ::sem_destroy(&semaphore_) == 0;
    }
    sem_t semaphore_{};
    bool opened_ = false;
};
} // namespace dp::osal
