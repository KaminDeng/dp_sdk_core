/** @file   test_uart.cpp
 *  @brief  GTest suite for dp_hal UART CRTP interface with MockUart. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_UART_CPP_
#define TEST_UART_CPP_

#include <gtest/gtest.h>

#include "dp_uart.h"
#include "mock_uart.h"

namespace {

class DpHalUartTest : public ::testing::Test {
protected:
    void SetUp() override { uart_.reset(); }

    dp::hal::mock::MockUart uart_;
};

TEST_F(DpHalUartTest, ConfigureSetsParameters) {
    dp::hal::UartConfig cfg{115200, 8, 1, 0};
    EXPECT_EQ(uart_.configure(cfg), dp::hal::Status::kOk);
    EXPECT_TRUE(uart_.wasConfigured());
    EXPECT_EQ(uart_.lastConfig().baud_rate, 115200u);
    EXPECT_EQ(uart_.lastConfig().data_bits, 8u);
    EXPECT_EQ(uart_.lastConfig().stop_bits, 1u);
    EXPECT_EQ(uart_.lastConfig().parity, 0u);
}

TEST_F(DpHalUartTest, WriteRecordsData) {
    const uint8_t data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    EXPECT_EQ(uart_.write(data, sizeof(data)), dp::hal::Status::kOk);
    EXPECT_EQ(uart_.writtenBytes(), sizeof(data));
    EXPECT_EQ(uart_.written()[0], 0x48);
    EXPECT_EQ(uart_.written()[4], 0x6F);
}

TEST_F(DpHalUartTest, WriteMultipleAccumulates) {
    const uint8_t d1[] = {0x01, 0x02};
    const uint8_t d2[] = {0x03};
    uart_.write(d1, sizeof(d1));
    uart_.write(d2, sizeof(d2));
    EXPECT_EQ(uart_.writtenBytes(), 3u);
    EXPECT_EQ(uart_.written()[2], 0x03);
}

TEST_F(DpHalUartTest, ReadReturnsStagedData) {
    const uint8_t staged[] = {0xAA, 0xBB, 0xCC};
    uart_.stageReadData(staged, sizeof(staged));

    uint8_t buf[3] = {};
    size_t actual = 0;
    EXPECT_EQ(uart_.read(buf, sizeof(buf), &actual), dp::hal::Status::kOk);
    EXPECT_EQ(actual, 3u);
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[1], 0xBB);
    EXPECT_EQ(buf[2], 0xCC);
}

TEST_F(DpHalUartTest, ReadPartialReturnsAvailable) {
    const uint8_t staged[] = {0x11, 0x22};
    uart_.stageReadData(staged, sizeof(staged));

    uint8_t buf[4] = {};
    size_t actual = 0;
    EXPECT_EQ(uart_.read(buf, sizeof(buf), &actual), dp::hal::Status::kOk);
    EXPECT_EQ(actual, 2u);
    EXPECT_EQ(buf[0], 0x11);
    EXPECT_EQ(buf[1], 0x22);
}

TEST_F(DpHalUartTest, ReadEmptyReturnsZero) {
    uint8_t buf[4] = {};
    size_t actual = 99;
    EXPECT_EQ(uart_.read(buf, sizeof(buf), &actual), dp::hal::Status::kOk);
    EXPECT_EQ(actual, 0u);
}

TEST_F(DpHalUartTest, FlushSetsFlag) {
    EXPECT_FALSE(uart_.wasFlushed());
    EXPECT_EQ(uart_.flush(), dp::hal::Status::kOk);
    EXPECT_TRUE(uart_.wasFlushed());
}

TEST_F(DpHalUartTest, AsyncCallbacksNotSupported) {
    EXPECT_EQ(uart_.setRxCallback(nullptr, nullptr), dp::hal::Status::kNotSupported);
    EXPECT_EQ(uart_.setTxCompleteCallback(nullptr, nullptr), dp::hal::Status::kNotSupported);
}

TEST_F(DpHalUartTest, DmaCapabilityQuery) {
    EXPECT_FALSE(dp::hal::mock::MockUart::kSupportsDma);
    EXPECT_EQ(dp::hal::mock::MockUart::kDmaAlignment, 0u);
}

TEST_F(DpHalUartTest, ResetClearsState) {
    const uint8_t data[] = {0x01};
    uart_.write(data, 1);
    uart_.flush();
    dp::hal::UartConfig cfg{9600, 8, 1, 0};
    uart_.configure(cfg);

    uart_.reset();

    EXPECT_EQ(uart_.writtenBytes(), 0u);
    EXPECT_FALSE(uart_.wasConfigured());
    EXPECT_FALSE(uart_.wasFlushed());
}

}  // namespace

#endif  // TEST_UART_CPP_
