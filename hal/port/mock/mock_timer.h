/** @file   mock_timer.h
 *  @brief  Mock timer implementation for dp_hal host testing. */

#ifndef MOCK_TIMER_H_
#define MOCK_TIMER_H_

#include "dp_timer.h"

namespace dp::hal::mock {

/** @brief  Mock timer for GTest verification. */
class MockTimer : public TimerBase<MockTimer> {
    friend class TimerBase<MockTimer>;

public:
    /** @brief  Reset all mock state. */
    void reset() {
        running_ = false;
        period_us_ = 0;
        callback_ = nullptr;
        callback_ctx_ = nullptr;
        counter_ = 0;
    }

    /** @brief  Whether the timer is currently running. */
    bool isRunning() const { return running_; }

    /** @brief  Get the configured period. */
    uint32_t periodUs() const { return period_us_; }

    /** @brief  Whether a callback was registered. */
    bool hasCallback() const { return callback_ != nullptr; }

    /** @brief  Advance the mock counter by a given amount.
     *  @param  us  Microseconds to advance. */
    void advanceCounter(uint32_t us) { counter_ += us; }

private:
    Status doStart(uint32_t period_us) {
        period_us_ = period_us;
        running_ = true;
        return Status::kOk;
    }

    Status doStop() {
        running_ = false;
        return Status::kOk;
    }

    Status doSetCallback(TimerBase::Callback cb, void *ctx) {
        callback_ = cb;
        callback_ctx_ = ctx;
        return Status::kOk;
    }

    uint32_t doGetCounterUs() {
        uint32_t val = counter_;
        counter_ += 100;  // Auto-increment for sequential reads.
        return val;
    }

    bool running_ = false;
    uint32_t period_us_ = 0;
    TimerBase::Callback callback_ = nullptr;
    void *callback_ctx_ = nullptr;
    uint32_t counter_ = 0;
};

}  // namespace dp::hal::mock

#endif  // MOCK_TIMER_H_
