//
// Created by kamin.deng on 2024/8/22.
//
#ifndef __OSAL_TIMER_H__
#define __OSAL_TIMER_H__

#include <functional>

#include "interface_timer.h"
#include "osal.h"
#include "osal_chrono.h"
#include "osal_debug.h"
#include "osal_lockguard.h"
#include "osal_mutex.h"

namespace osal {

class OSALTimer : public ITimer {
public:
    OSALTimer() : timerId_(nullptr), running_(false), periodic_(false), interval_(0) {}

    ~OSALTimer() { stop(); }

    void start(uint32_t interval, bool periodic, std::function<void()> callback) override {
        stop();  // 停止任何现有的定时器
        interval_ = interval;
        periodic_ = periodic;
        callback_ = callback;

        osTimerType_t timerType = periodic ? osTimerPeriodic : osTimerOnce;
        osTimerAttr_t timerAttr = {};
        timerAttr.name = "OSALTimer";

        timerId_ = osTimerNew(&OSALTimer::timerCallback, timerType, this, &timerAttr);
        if (timerId_ == nullptr) {
            OSAL_LOGE("Failed to create timer\n");
            return;
        }

        endTime_ = OSALChrono::getInstance().now() + interval_;
        if (osTimerStart(timerId_, interval) != osOK) {
            OSAL_LOGE("Failed to start timer\n");
            osTimerDelete(timerId_);
            timerId_ = nullptr;
            return;
        }

        running_ = true;
        OSAL_LOGD("Timer started\n");
    }

    void stop() override {
        if (timerId_ != nullptr) {
            osTimerStop(timerId_);
            osTimerDelete(timerId_);
            timerId_ = nullptr;
            running_ = false;
            OSAL_LOGD("Timer stopped\n");
        }
    }

    bool isRunning() const override { return running_; }

    uint32_t getRemainingTime() const override {
        // CMSIS-RTOS2 并没有直接提供查询剩余时间的接口
        // 这里可以通过记录开始时间和间隔时间来计算剩余时间
        OSALLockGuard lockGuard(mutex_);
        if (!running_) {
            return 0;
        }

        auto now = OSALChrono::getInstance().now();
        auto remaining = (endTime_ - now);
        return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
    }

    void reset() override {
        OSALLockGuard lockGuard(mutex_);
        if (running_) {
            osTimerStop(timerId_);
            osTimerStart(timerId_, interval_);
            endTime_ = OSALChrono::getInstance().now() + interval_;
            OSAL_LOGD("Timer reset\n");
        }
    }

private:
    static void timerCallback(void *arg) {
        OSALTimer *timer = static_cast<OSALTimer *>(arg);
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
    mutable OSALMutex mutex_;
    OSALChrono::TimePoint endTime_;
};

}  // namespace osal

#endif  // __OSAL_TIMER_H__