/** @file   test_serial_device.cpp
 *  @brief  GTest suite for SerialDevice adapter. */

#include <gtest/gtest.h>

#include "dp_serial_device.h"
#include "mock_uart.h"

namespace {

using dp::device::DeviceType;
using dp::device::SerialDevice;
using dp::hal::Status;
using dp::hal::UartConfig;
using dp::hal::mock::MockUart;

class DpSerialDeviceTest : public ::testing::Test {
protected:
    void SetUp() override { mock_.reset(); }

    MockUart mock_;
    SerialDevice<MockUart> dev_{"uart0", mock_};
};

TEST_F(DpSerialDeviceTest, TypeIsSerial) {
    EXPECT_EQ(dev_.type(), DeviceType::kSerial);
    EXPECT_STREQ(dev_.name(), "uart0");
}

TEST_F(DpSerialDeviceTest, WriteDelegatesToHal) {
    const uint8_t data[] = {0x41, 0x42};
    EXPECT_EQ(dev_.write(data, sizeof(data)), Status::kOk);
    EXPECT_EQ(mock_.writtenBytes(), sizeof(data));
}

TEST_F(DpSerialDeviceTest, ReadDelegatesToHal) {
    const uint8_t stage[] = {0x01, 0x02, 0x03};
    mock_.stageReadData(stage, sizeof(stage));

    uint8_t buf[4] = {};
    size_t actual = 0;
    EXPECT_EQ(dev_.read(buf, sizeof(buf), &actual), Status::kOk);
    EXPECT_EQ(actual, sizeof(stage));
}

TEST_F(DpSerialDeviceTest, ConfigureDelegatesToHal) {
    UartConfig cfg{115200, 8, 1, 0};
    EXPECT_EQ(dev_.configure(cfg), Status::kOk);
    EXPECT_TRUE(mock_.wasConfigured());
    EXPECT_EQ(mock_.lastConfig().baud_rate, 115200U);
}

TEST_F(DpSerialDeviceTest, CanCastToIUart) {
    dp::hal::IUart *uart = &dev_;
    EXPECT_NE(uart, nullptr);
    const uint8_t data[] = {0xFF};
    EXPECT_EQ(uart->write(data, 1), Status::kOk);
    EXPECT_EQ(mock_.writtenBytes(), 1U);
}

}  // namespace
