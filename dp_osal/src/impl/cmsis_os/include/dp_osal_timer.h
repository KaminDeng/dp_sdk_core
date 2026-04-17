//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_TIMER_H_
#define DP_OSAL_CMSIS_TIMER_H_

#if DP_OSAL_ENABLE_TIMER

#include <atomic>
#include <functional>

#include "interface_timer.h"
#include "dp_osal_port.h"
#include "dp_osal_chrono.h"
#include "dp_osal_debug.h"
#include "dp_osal_lockguard.h"
#include "dp_osal_mutex.h"

namespace dp::osal {

class Timer : public TimerBase<Timer> {
    friend class TimerBase<Timer>;

public:
    Timer() : timerId_(nullptr), running_(false), periodic_(false), interval_(0) {}

    ~Timer() { stop(); }

private:
    void doStart(uint32_t interval, bool periodic, std::function<void()> callback) {
        stop();  // 停止任何现有的定时器
        interval_ = interval;
        periodic_ = periodic;
        callback_ = callback;

        osTimerType_t timerType = periodic ? osTimerPeriodic : osTimerOnce;
        osTimerAttr_t timerAttr = {};
        timerAttr.name = "Timer";

        timerId_ = osTimerNew(&Timer::timerCallback, timerType, this, &timerAttr);
        if (timerId_ == nullptr) {
            DP_OSAL_LOGE("Failed to create timer\n");
            return;
        }

        endTime_ = Chrono::getInstance().now() + interval_;
        if (osTimerStart(timerId_, interval) != osOK) {
            DP_OSAL_LOGE("Failed to start timer\n");
            osTimerDelete(timerId_);
            timerId_ = nullptr;
            return;
        }

        running_ = true;
        DP_OSAL_LOGD("Timer started\n");
    }

    void doStop() {
        if (timerId_ != nullptr) {
            osTimerStop(timerId_);
            osTimerDelete(timerId_);
            timerId_ = nullptr;
            running_ = false;
            DP_OSAL_LOGD("Timer stopped\n");
        }
    }

    bool doIsRunning() const { return running_; }

    uint32_t doGetRemainingTime() const {
        // CMSIS-RTOS2 并没有直接提供查询剩余时间的接口
        // 这里可以通过记录开始时间和间隔时间来计算剩余时间
        LockGuard lockGuard(mutex_);
        if (!running_) {
            return 0;
        }

        auto now = Chrono::getInstance().now();
        auto remaining = (endTime_ - now);
        return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
    }

    void doReset() {
        LockGuard lockGuard(mutex_);
        if (running_) {
            osTimerStop(timerId_);
            osTimerStart(timerId_, interval_);
            endTime_ = Chrono::getInstance().now() + interval_;
            DP_OSAL_LOGD("Timer reset\n");
        }
    }

    static void timerCallback(void *arg) {
        Timer *timer = static_cast<Timer *>(arg);
        if (timer->callback_) {
            timer->callback_();
        }
        if (!timer->periodic_) {
            timer->running_ = false;
        }
    }

    osTimerId_t timerId_;
    std::atomic<bool> running_;
    bool periodic_;
    uint32_t interval_;
    std::function<void()> callback_;
    mutable Mutex mutex_;
    Chrono::TimePoint endTime_;
};

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_TIMER */

#endif  // DP_OSAL_CMSIS_TIMER_H_
