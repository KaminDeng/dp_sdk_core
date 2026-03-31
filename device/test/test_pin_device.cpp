/** @file   test_pin_device.cpp
 *  @brief  GTest suite for PinDevice adapter. */

#include <gtest/gtest.h>

#include "dp_pin_device.h"
#include "mock_gpio.h"

namespace {

using dp::device::DeviceType;
using dp::device::PinDevice;
using dp::hal::PinState;
using dp::hal::Status;
using dp::hal::mock::MockGpioPin;

class DpPinDeviceTest : public ::testing::Test {
protected:
    void SetUp() override { mock_.reset(); }

    MockGpioPin mock_;
    PinDevice<MockGpioPin> dev_{"led0", mock_};
};

TEST_F(DpPinDeviceTest, TypeIsPin) {
    EXPECT_EQ(dev_.type(), DeviceType::kPin);
    EXPECT_STREQ(dev_.name(), "led0");
}

TEST_F(DpPinDeviceTest, WriteDelegatesToHal) {
    EXPECT_EQ(dev_.write(PinState::kActive), Status::kOk);
    EXPECT_EQ(mock_.currentState(), PinState::kActive);
}

TEST_F(DpPinDeviceTest, ReadDelegatesToHal) {
    mock_.write(PinState::kActive);
    EXPECT_EQ(dev_.read(), PinState::kActive);
}

TEST_F(DpPinDeviceTest, CanCastToIGpioPin) {
    dp::hal::IGpioPin *pin = &dev_;
    EXPECT_NE(pin, nullptr);
    EXPECT_EQ(pin->write(PinState::kActive), Status::kOk);
    EXPECT_EQ(mock_.currentState(), PinState::kActive);
}

}  // namespace
