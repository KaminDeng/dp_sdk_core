/** @file   test_can.cpp
 *  @brief  GTest suite for dp_hal CAN CRTP interface with MockCan. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_CAN_CPP_
#define TEST_CAN_CPP_

#include <gtest/gtest.h>

#include "dp_can.h"
#include "mock_can.h"

namespace {

class DpHalCanTest : public ::testing::Test {
protected:
    void SetUp() override { can_.reset(); }

    dp::hal::mock::MockCan can_;
};

TEST_F(DpHalCanTest, ConfigureSetsConfiguredFlag) {
    dp::hal::CanConfig cfg{};
    cfg.bitrate = 500000;
    cfg.mode = 0;
    EXPECT_EQ(can_.configure(cfg), dp::hal::Status::kOk);
    EXPECT_TRUE(can_.isConfigured());
}

TEST_F(DpHalCanTest, SendAppendsFrameToTxQueue) {
    dp::hal::CanFrame frame{};
    frame.id = 0x123;
    frame.dlc = 3;
    frame.data[0] = 0x11;
    frame.data[1] = 0x22;
    frame.data[2] = 0x33;
    frame.is_extended = false;
    frame.is_remote = false;

    EXPECT_EQ(can_.send(frame), dp::hal::Status::kOk);
    ASSERT_EQ(can_.txFrames().size(), 1u);
    EXPECT_EQ(can_.txFrames()[0].id, 0x123u);
    EXPECT_EQ(can_.txFrames()[0].dlc, 3u);
    EXPECT_EQ(can_.txFrames()[0].data[0], 0x11u);
}

TEST_F(DpHalCanTest, ReceiveReturnsStagedFrame) {
    dp::hal::CanFrame staged{};
    staged.id = 0x456;
    staged.dlc = 2;
    staged.data[0] = 0xAA;
    staged.data[1] = 0xBB;
    staged.is_extended = true;
    staged.is_remote = false;
    can_.stageRxFrame(staged);

    dp::hal::CanFrame out{};
    EXPECT_EQ(can_.receive(&out), dp::hal::Status::kOk);
    EXPECT_EQ(out.id, 0x456u);
    EXPECT_EQ(out.dlc, 2u);
    EXPECT_EQ(out.data[0], 0xAAu);
    EXPECT_EQ(out.data[1], 0xBBu);
    EXPECT_TRUE(out.is_extended);
}

TEST_F(DpHalCanTest, ReceiveWithoutStagedFrameReturnsError) {
    dp::hal::CanFrame out{};
    EXPECT_EQ(can_.receive(&out), dp::hal::Status::kError);
}

TEST_F(DpHalCanTest, SetFilterReturnsOk) {
    EXPECT_EQ(can_.setFilter(0x100, 0x7FF, false), dp::hal::Status::kOk);
}

TEST_F(DpHalCanTest, SetRxCallbackReportsNotSupportedInMock) {
    EXPECT_EQ(can_.setRxCallback(nullptr, nullptr), dp::hal::Status::kNotSupported);
}

}  // namespace

#endif  // TEST_CAN_CPP_
