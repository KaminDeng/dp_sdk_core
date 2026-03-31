/** @file   test_gpio.cpp
 *  @brief  GTest suite for dp_hal GPIO CRTP interface with MockGpioPin. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_GPIO_CPP_
#define TEST_GPIO_CPP_

#include <gtest/gtest.h>

#include "dp_gpio.h"
#include "mock_gpio.h"

namespace {

class DpHalGpioTest : public ::testing::Test {
protected:
    void SetUp() override { pin_.reset(); }

    dp::hal::mock::MockGpioPin pin_;
};

TEST_F(DpHalGpioTest, SetMode) {
    EXPECT_FALSE(pin_.wasModeSet());
    EXPECT_EQ(pin_.setMode(dp::hal::PinMode::kOutput), dp::hal::Status::kOk);
    EXPECT_TRUE(pin_.wasModeSet());
    EXPECT_EQ(pin_.currentMode(), dp::hal::PinMode::kOutput);
}

TEST_F(DpHalGpioTest, WriteAndRead) {
    EXPECT_EQ(pin_.read(), dp::hal::PinState::kInactive);
    EXPECT_EQ(pin_.write(dp::hal::PinState::kActive), dp::hal::Status::kOk);
    EXPECT_EQ(pin_.read(), dp::hal::PinState::kActive);
    EXPECT_EQ(pin_.write(dp::hal::PinState::kInactive), dp::hal::Status::kOk);
    EXPECT_EQ(pin_.read(), dp::hal::PinState::kInactive);
}

TEST_F(DpHalGpioTest, Toggle) {
    EXPECT_EQ(pin_.read(), dp::hal::PinState::kInactive);
    EXPECT_EQ(pin_.toggle(), dp::hal::Status::kOk);
    EXPECT_EQ(pin_.read(), dp::hal::PinState::kActive);
    EXPECT_EQ(pin_.toggle(), dp::hal::Status::kOk);
    EXPECT_EQ(pin_.read(), dp::hal::PinState::kInactive);
}

TEST_F(DpHalGpioTest, StateHistoryRecorded) {
    pin_.write(dp::hal::PinState::kActive);
    pin_.write(dp::hal::PinState::kInactive);
    pin_.toggle();

    const auto &history = pin_.stateHistory();
    ASSERT_EQ(history.size(), 3u);
    EXPECT_EQ(history[0], dp::hal::PinState::kActive);
    EXPECT_EQ(history[1], dp::hal::PinState::kInactive);
    EXPECT_EQ(history[2], dp::hal::PinState::kActive);
}

TEST_F(DpHalGpioTest, IrqNotSupported) {
    EXPECT_EQ(pin_.enableIrq(dp::hal::GpioIrqTrigger::kRising, nullptr, nullptr), dp::hal::Status::kNotSupported);
    EXPECT_EQ(pin_.disableIrq(), dp::hal::Status::kNotSupported);
}

}  // namespace

#endif  // TEST_GPIO_CPP_
