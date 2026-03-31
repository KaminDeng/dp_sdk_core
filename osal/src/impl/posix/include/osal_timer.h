//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_TIMER_H__
#define __OSAL_TIMER_H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <thread>

#include "interface_timer.h"
#include "osal_debug.h"

namespace osal {

class OSALTimer : public ITimer {
public:
    OSALTimer() : running_(false), periodic_(false), interval_(0) {}

    ~OSALTimer() { stop(); }

    void start(uint32_t interval, bool periodic, std::function<void()> callback) override {
        stop();  // 停止任何现有的定时器
        interval_ = interval;
        periodic_ = periodic;
        callback_ = callback;
        running_ = true;
        thread_ = std::thread(&OSALTimer::run, this);
        OSAL_LOGD("Timer started\n");
    }

    void stop() override {
        running_ = false;
        cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
        OSAL_LOGD("Timer stopped\n");
    }

    bool isRunning() const override { return running_; }

    uint32_t getRemainingTime() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return 0;
        }
        auto now = std::chrono::steady_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(endTime_ - now);
        return remaining.count() > 0 ? static_cast<uint32_t>(remaining.count()) : 0;
    }

    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            endTime_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_);
            cv_.notify_all();
        }
        OSAL_LOGD("Timer reset\n");
    }

private:
    void run() {
        std::unique_lock<std::mutex> lock(mutex_);
        endTime_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_);
        while (running_) {
            if (cv_.wait_until(lock, endTime_) == std::cv_status::timeout) {
                if (callback_) {
                    callback_();
                }
                if (periodic_) {
                    endTime_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_);
                } else {
                    running_ = false;
                }
            }
        }
    }

    std::atomic<bool> running_;
    bool periodic_;
    uint32_t interval_;
    std::function<void()> callback_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::chrono::steady_clock::time_point endTime_;
};

}  // namespace osal

#endif  // __OSAL_TIMER_H__