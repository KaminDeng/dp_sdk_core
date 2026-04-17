/** @file   mock_pwm.h
 *  @brief  Mock PWM implementation for dp_hal host testing. */

#ifndef MOCK_PWM_H_
#define MOCK_PWM_H_

#include "dp_pwm.h"

namespace dp::hal::mock {

/** @brief  Mock PWM channel for GTest verification. */
class MockPwm : public PwmBase<MockPwm> {
    friend class PwmBase<MockPwm>;

public:
    /** @brief  Reset all mock state. */
    void reset() {
        running_ = false;
        frequency_hz_ = 0;
        duty_percent_ = 0.0f;
        start_count_ = 0;
        stop_count_ = 0;
    }

    /** @brief  Whether the PWM output is active. */
    bool isRunning() const { return running_; }

    /** @brief  Get the configured frequency. */
    uint32_t frequencyHz() const { return frequency_hz_; }

    /** @brief  Get the configured duty cycle. */
    float dutyPercent() const { return duty_percent_; }

    /** @brief  Number of start() calls since reset. */
    size_t startCount() const { return start_count_; }

    /** @brief  Number of stop() calls since reset. */
    size_t stopCount() const { return stop_count_; }

private:
    Status doStart(uint32_t frequency_hz, float duty_percent) {
        if (duty_percent < 0.0f || duty_percent > 100.0f) {
            return Status::kInvalidArg;
        }
        if (frequency_hz == 0) {
            return Status::kInvalidArg;
        }
        frequency_hz_ = frequency_hz;
        duty_percent_ = duty_percent;
        running_ = true;
        ++start_count_;
        return Status::kOk;
    }

    Status doSetDuty(float duty_percent) {
        if (duty_percent < 0.0f || duty_percent > 100.0f) {
            return Status::kInvalidArg;
        }
        duty_percent_ = duty_percent;
        return Status::kOk;
    }

    Status doSetFrequency(uint32_t frequency_hz) {
        if (frequency_hz == 0) {
            return Status::kInvalidArg;
        }
        frequency_hz_ = frequency_hz;
        return Status::kOk;
    }

    Status doStop() {
        running_ = false;
        ++stop_count_;
        return Status::kOk;
    }

    bool running_ = false;
    uint32_t frequency_hz_ = 0;
    float duty_percent_ = 0.0f;
    size_t start_count_ = 0;
    size_t stop_count_ = 0;
};

}  // namespace dp::hal::mock

#endif  // MOCK_PWM_H_
