/** @file   test_timer.cpp
 *  @brief  GTest suite for dp_hal Timer CRTP interface with MockTimer. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_TIMER_CPP_
#define TEST_TIMER_CPP_

#include <gtest/gtest.h>

#include "dp_timer.h"
#include "mock_timer.h"

namespace {

class DpHalTimerTest : public ::testing::Test {
protected:
    void SetUp() override { timer_.reset(); }

    dp::hal::mock::MockTimer timer_;
};

TEST_F(DpHalTimerTest, StartSetsRunning) {
    EXPECT_FALSE(timer_.isRunning());
    EXPECT_EQ(timer_.start(1000), dp::hal::Status::kOk);
    EXPECT_TRUE(timer_.isRunning());
    EXPECT_EQ(timer_.periodUs(), 1000u);
}

TEST_F(DpHalTimerTest, StopClearsRunning) {
    timer_.start(5000);
    EXPECT_TRUE(timer_.isRunning());
    EXPECT_EQ(timer_.stop(), dp::hal::Status::kOk);
    EXPECT_FALSE(timer_.isRunning());
}

TEST_F(DpHalTimerTest, SetCallbackRecords) {
    EXPECT_FALSE(timer_.hasCallback());
    auto cb = [](void *) {};
    EXPECT_EQ(timer_.setCallback(cb, nullptr), dp::hal::Status::kOk);
    EXPECT_TRUE(timer_.hasCallback());
}

TEST_F(DpHalTimerTest, GetCounterUsIncrements) {
    uint32_t v1 = timer_.getCounterUs();
    uint32_t v2 = timer_.getCounterUs();
    EXPECT_LT(v1, v2);
}

TEST_F(DpHalTimerTest, AdvanceCounter) {
    timer_.advanceCounter(5000);
    uint32_t val = timer_.getCounterUs();
    EXPECT_EQ(val, 5000u);
}

TEST_F(DpHalTimerTest, ResetClearsState) {
    timer_.start(1000);
    auto cb = [](void *) {};
    timer_.setCallback(cb, nullptr);
    timer_.advanceCounter(999);

    timer_.reset();

    EXPECT_FALSE(timer_.isRunning());
    EXPECT_EQ(timer_.periodUs(), 0u);
    EXPECT_FALSE(timer_.hasCallback());
    EXPECT_EQ(timer_.getCounterUs(), 0u);
}

}  // namespace

#endif  // TEST_TIMER_CPP_
