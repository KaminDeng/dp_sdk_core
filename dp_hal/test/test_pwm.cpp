/** @file   test_pwm.cpp
 *  @brief  GTest suite for dp_hal PWM CRTP interface with MockPwm. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_PWM_CPP_
#define TEST_PWM_CPP_

#include <gtest/gtest.h>

#include "dp_pwm.h"
#include "mock_pwm.h"

namespace {

class DpHalPwmTest : public ::testing::Test {
protected:
    void SetUp() override { pwm_.reset(); }

    dp::hal::mock::MockPwm pwm_;
};

TEST_F(DpHalPwmTest, StartSetsPwm) {
    EXPECT_FALSE(pwm_.isRunning());
    EXPECT_EQ(pwm_.start(1000, 50.0f), dp::hal::Status::kOk);
    EXPECT_TRUE(pwm_.isRunning());
    EXPECT_EQ(pwm_.frequencyHz(), 1000u);
    EXPECT_FLOAT_EQ(pwm_.dutyPercent(), 50.0f);
}

TEST_F(DpHalPwmTest, SetDutyUpdates) {
    pwm_.start(1000, 50.0f);
    EXPECT_EQ(pwm_.setDuty(75.0f), dp::hal::Status::kOk);
    EXPECT_FLOAT_EQ(pwm_.dutyPercent(), 75.0f);
    EXPECT_EQ(pwm_.frequencyHz(), 1000u);
}

TEST_F(DpHalPwmTest, SetFrequencyUpdates) {
    pwm_.start(1000, 50.0f);
    EXPECT_EQ(pwm_.setFrequency(2700), dp::hal::Status::kOk);
    EXPECT_EQ(pwm_.frequencyHz(), 2700u);
    EXPECT_FLOAT_EQ(pwm_.dutyPercent(), 50.0f);
}

TEST_F(DpHalPwmTest, StopClearsPwm) {
    pwm_.start(1000, 50.0f);
    EXPECT_EQ(pwm_.stop(), dp::hal::Status::kOk);
    EXPECT_FALSE(pwm_.isRunning());
}

TEST_F(DpHalPwmTest, InvalidDutyTooHigh) {
    EXPECT_EQ(pwm_.start(1000, 101.0f), dp::hal::Status::kInvalidArg);
    EXPECT_FALSE(pwm_.isRunning());
}

TEST_F(DpHalPwmTest, InvalidDutyNegative) {
    EXPECT_EQ(pwm_.start(1000, -1.0f), dp::hal::Status::kInvalidArg);
}

TEST_F(DpHalPwmTest, InvalidFrequencyZero) {
    EXPECT_EQ(pwm_.start(0, 50.0f), dp::hal::Status::kInvalidArg);
}

TEST_F(DpHalPwmTest, SetDutyInvalidRange) {
    pwm_.start(1000, 50.0f);
    EXPECT_EQ(pwm_.setDuty(101.0f), dp::hal::Status::kInvalidArg);
    EXPECT_FLOAT_EQ(pwm_.dutyPercent(), 50.0f);
}

TEST_F(DpHalPwmTest, SetFrequencyInvalidZero) {
    pwm_.start(1000, 50.0f);
    EXPECT_EQ(pwm_.setFrequency(0), dp::hal::Status::kInvalidArg);
    EXPECT_EQ(pwm_.frequencyHz(), 1000u);
}

TEST_F(DpHalPwmTest, ResetClearsState) {
    pwm_.start(2700, 80.0f);
    pwm_.stop();
    pwm_.reset();
    EXPECT_FALSE(pwm_.isRunning());
    EXPECT_EQ(pwm_.frequencyHz(), 0u);
    EXPECT_FLOAT_EQ(pwm_.dutyPercent(), 0.0f);
    EXPECT_EQ(pwm_.startCount(), 0u);
    EXPECT_EQ(pwm_.stopCount(), 0u);
}

TEST_F(DpHalPwmTest, StartStopCountTracking) {
    pwm_.start(1000, 50.0f);
    pwm_.stop();
    pwm_.start(2000, 25.0f);
    pwm_.stop();
    EXPECT_EQ(pwm_.startCount(), 2u);
    EXPECT_EQ(pwm_.stopCount(), 2u);
}

TEST_F(DpHalPwmTest, BoundaryDutyZeroAndHundred) {
    EXPECT_EQ(pwm_.start(1000, 0.0f), dp::hal::Status::kOk);
    EXPECT_FLOAT_EQ(pwm_.dutyPercent(), 0.0f);
    EXPECT_EQ(pwm_.setDuty(100.0f), dp::hal::Status::kOk);
    EXPECT_FLOAT_EQ(pwm_.dutyPercent(), 100.0f);
}

}  // namespace

#endif  // TEST_PWM_CPP_
