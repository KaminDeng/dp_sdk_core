/** @file   test_spi.cpp
 *  @brief  GTest suites for dp_hal SPI bus and SPI device interfaces. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_SPI_CPP_
#define TEST_SPI_CPP_

#include <gtest/gtest.h>

#include "dp_gpio.h"
#include "dp_spi.h"
#include "mock_gpio.h"
#include "mock_spi.h"

namespace {

/* ---- SpiBusBase tests -------------------------------------------------- */

class DpHalSpiBusTest : public ::testing::Test {
protected:
    void SetUp() override { spi_.reset(); }

    dp::hal::mock::MockSpiBus spi_;
};

TEST_F(DpHalSpiBusTest, ConfigureSetsParameters) {
    dp::hal::SpiConfig cfg{1000000, 0, 0};
    EXPECT_EQ(spi_.configure(cfg), dp::hal::Status::kOk);
    EXPECT_TRUE(spi_.wasConfigured());
    EXPECT_EQ(spi_.lastConfig().clock_hz, 1000000u);
}

TEST_F(DpHalSpiBusTest, TransferRecordsTxData) {
    const uint8_t tx[] = {0xDE, 0xAD};
    uint8_t rx[2] = {};
    EXPECT_EQ(spi_.transfer(tx, rx, 2), dp::hal::Status::kOk);
    ASSERT_EQ(spi_.transfers().size(), 1u);
    EXPECT_EQ(spi_.transfers()[0].tx_data[0], 0xDE);
    EXPECT_EQ(spi_.transfers()[0].tx_data[1], 0xAD);
}

TEST_F(DpHalSpiBusTest, TransferReturnsStagedRxData) {
    const uint8_t staged[] = {0xBE, 0xEF};
    spi_.stageRxData(staged, 2);

    const uint8_t tx[] = {0x00, 0x00};
    uint8_t rx[2] = {};
    spi_.transfer(tx, rx, 2);
    EXPECT_EQ(rx[0], 0xBE);
    EXPECT_EQ(rx[1], 0xEF);
}

TEST_F(DpHalSpiBusTest, WriteOnly) {
    const uint8_t buf[] = {0x01, 0x02, 0x03};
    EXPECT_EQ(spi_.write(buf, 3), dp::hal::Status::kOk);
    ASSERT_EQ(spi_.transfers().size(), 1u);
    EXPECT_EQ(spi_.transfers()[0].len, 3u);
}

TEST_F(DpHalSpiBusTest, ReadOnly) {
    const uint8_t staged[] = {0xAA};
    spi_.stageRxData(staged, 1);

    uint8_t buf[1] = {};
    EXPECT_EQ(spi_.read(buf, 1), dp::hal::Status::kOk);
    EXPECT_EQ(buf[0], 0xAA);
}

TEST_F(DpHalSpiBusTest, TransferAsyncNotSupported) {
    EXPECT_EQ(spi_.transferAsync(nullptr, nullptr, 0, nullptr, nullptr), dp::hal::Status::kNotSupported);
}

TEST_F(DpHalSpiBusTest, DmaCapabilityQuery) {
    EXPECT_FALSE(dp::hal::mock::MockSpiBus::kSupportsDma);
    EXPECT_EQ(dp::hal::mock::MockSpiBus::kDmaAlignment, 0u);
}

/* ---- SpiDevice tests --------------------------------------------------- */

class DpHalSpiDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        spi_.reset();
        cs_.reset();
        cs_.setMode(dp::hal::PinMode::kOutput);
    }

    dp::hal::mock::MockSpiBus spi_;
    dp::hal::mock::MockGpioPin cs_;
};

TEST_F(DpHalSpiDeviceTest, TransferTogglesCs) {
    dp::hal::SpiDevice<dp::hal::mock::MockSpiBus, dp::hal::mock::MockGpioPin> dev(spi_, cs_);

    const uint8_t tx[] = {0x42};
    uint8_t rx[1] = {};
    EXPECT_EQ(dev.transfer(tx, rx, 1), dp::hal::Status::kOk);

    // CS history: Active (assert) -> Inactive (deassert)
    const auto &history = cs_.stateHistory();
    // stateHistory() from SetUp has 0 entries from setMode (setMode
    // doesn't record to stateHistory). The first write is from
    // SpiDevice.
    ASSERT_GE(history.size(), 2u);
    size_t n = history.size();
    EXPECT_EQ(history[n - 2], dp::hal::PinState::kActive);
    EXPECT_EQ(history[n - 1], dp::hal::PinState::kInactive);
}

TEST_F(DpHalSpiDeviceTest, WriteTogglesCs) {
    dp::hal::SpiDevice<dp::hal::mock::MockSpiBus, dp::hal::mock::MockGpioPin> dev(spi_, cs_);

    const uint8_t buf[] = {0x01};
    EXPECT_EQ(dev.write(buf, 1), dp::hal::Status::kOk);

    const auto &history = cs_.stateHistory();
    ASSERT_GE(history.size(), 2u);
    size_t n = history.size();
    EXPECT_EQ(history[n - 2], dp::hal::PinState::kActive);
    EXPECT_EQ(history[n - 1], dp::hal::PinState::kInactive);
}

TEST_F(DpHalSpiDeviceTest, ReadTogglesCs) {
    dp::hal::SpiDevice<dp::hal::mock::MockSpiBus, dp::hal::mock::MockGpioPin> dev(spi_, cs_);

    uint8_t buf[1] = {};
    EXPECT_EQ(dev.read(buf, 1), dp::hal::Status::kOk);

    const auto &history = cs_.stateHistory();
    ASSERT_GE(history.size(), 2u);
    size_t n = history.size();
    EXPECT_EQ(history[n - 2], dp::hal::PinState::kActive);
    EXPECT_EQ(history[n - 1], dp::hal::PinState::kInactive);
}

}  // namespace

#endif  // TEST_SPI_CPP_
