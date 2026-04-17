/** @file   mock_gpio.h
 *  @brief  Mock GPIO pin implementation for dp_hal host testing. */

#ifndef MOCK_GPIO_H_
#define MOCK_GPIO_H_

#include <vector>

#include "dp_gpio.h"

namespace dp::hal::mock {

/** @brief  Mock GPIO pin for GTest verification. */
class MockGpioPin : public GpioPinBase<MockGpioPin> {
    friend class GpioPinBase<MockGpioPin>;

public:
    /** @brief  Reset all mock state. */
    void reset() {
        state_ = PinState::kInactive;
        mode_ = PinMode::kInput;
        mode_set_ = false;
        state_history_.clear();
    }

    /** @brief  Get the current pin state. */
    PinState currentState() const { return state_; }

    /** @brief  Get the current pin mode. */
    PinMode currentMode() const { return mode_; }

    /** @brief  Whether setMode() was called. */
    bool wasModeSet() const { return mode_set_; }

    /** @brief  Get the history of all state writes (including toggles). */
    const std::vector<PinState> &stateHistory() const { return state_history_; }

private:
    Status doSetMode(PinMode mode) {
        mode_ = mode;
        mode_set_ = true;
        return Status::kOk;
    }

    Status doWrite(PinState state) {
        state_ = state;
        state_history_.push_back(state);
        return Status::kOk;
    }

    PinState doRead() { return state_; }

    Status doToggle() {
        state_ = (state_ == PinState::kActive) ? PinState::kInactive : PinState::kActive;
        state_history_.push_back(state_);
        return Status::kOk;
    }

    Status doEnableIrq(GpioIrqTrigger /*trigger*/, GpioPinBase::IrqCallback /*cb*/, void * /*ctx*/) {
        return Status::kNotSupported;
    }

    Status doDisableIrq() { return Status::kNotSupported; }

    PinState state_ = PinState::kInactive;
    PinMode mode_ = PinMode::kInput;
    bool mode_set_ = false;
    std::vector<PinState> state_history_;
};

}  // namespace dp::hal::mock

#endif  // MOCK_GPIO_H_
