/** @file   test_i2c.cpp
 *  @brief  GTest suites for dp_hal I2C bus and I2C device interfaces. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_I2C_CPP_
#define TEST_I2C_CPP_

#include <gtest/gtest.h>

#include "dp_i2c.h"
#include "mock_i2c.h"

namespace {

/* ---- I2cBusBase tests -------------------------------------------------- */

class DpHalI2cBusTest : public ::testing::Test {
protected:
    void SetUp() override { i2c_.reset(); }

    dp::hal::mock::MockI2cBus i2c_;
};

TEST_F(DpHalI2cBusTest, ConfigureSetsParameters) {
    dp::hal::I2cConfig cfg{400000};
    EXPECT_EQ(i2c_.configure(cfg), dp::hal::Status::kOk);
    EXPECT_TRUE(i2c_.wasConfigured());
    EXPECT_EQ(i2c_.lastConfig().clock_hz, 400000u);
}

TEST_F(DpHalI2cBusTest, WriteRecordsData) {
    const uint8_t data[] = {0x10, 0x20};
    EXPECT_EQ(i2c_.write(0x50, data, 2), dp::hal::Status::kOk);
    ASSERT_EQ(i2c_.writeRecords().size(), 1u);
    EXPECT_EQ(i2c_.writeRecords()[0].addr, 0x50);
    EXPECT_EQ(i2c_.writeRecords()[0].data[0], 0x10);
    EXPECT_EQ(i2c_.writeRecords()[0].data[1], 0x20);
}

TEST_F(DpHalI2cBusTest, ReadReturnsStagedData) {
    const uint8_t staged[] = {0xAA, 0xBB};
    i2c_.stageReadData(0x50, staged, 2);

    uint8_t buf[2] = {};
    EXPECT_EQ(i2c_.read(0x50, buf, 2), dp::hal::Status::kOk);
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[1], 0xBB);
}

TEST_F(DpHalI2cBusTest, ReadUnstagedReturnsZero) {
    uint8_t buf[2] = {0xFF, 0xFF};
    EXPECT_EQ(i2c_.read(0x50, buf, 2), dp::hal::Status::kOk);
    EXPECT_EQ(buf[0], 0x00);
    EXPECT_EQ(buf[1], 0x00);
}

TEST_F(DpHalI2cBusTest, WriteRead) {
    const uint8_t staged[] = {0xCC};
    i2c_.stageReadData(0x68, staged, 1);

    const uint8_t tx[] = {0x0F};
    uint8_t rx[1] = {};
    EXPECT_EQ(i2c_.writeRead(0x68, tx, 1, rx, 1), dp::hal::Status::kOk);
    EXPECT_EQ(rx[0], 0xCC);

    // writeRead should record the tx portion as a write
    ASSERT_GE(i2c_.writeRecords().size(), 1u);
    EXPECT_EQ(i2c_.writeRecords().back().addr, 0x68);
}

/* ---- I2cDevice tests --------------------------------------------------- */

class DpHalI2cDeviceTest : public ::testing::Test {
protected:
    void SetUp() override { i2c_.reset(); }

    dp::hal::mock::MockI2cBus i2c_;
};

TEST_F(DpHalI2cDeviceTest, WriteReg) {
    dp::hal::I2cDevice<dp::hal::mock::MockI2cBus> dev(i2c_, 0x50);

    const uint8_t data[] = {0xAA, 0xBB};
    EXPECT_EQ(dev.writeReg(0x10, data, 2), dp::hal::Status::kOk);

    ASSERT_EQ(i2c_.writeRecords().size(), 1u);
    const auto &rec = i2c_.writeRecords()[0];
    EXPECT_EQ(rec.addr, 0x50);
    // First byte is the register address
    ASSERT_EQ(rec.data.size(), 3u);
    EXPECT_EQ(rec.data[0], 0x10);
    EXPECT_EQ(rec.data[1], 0xAA);
    EXPECT_EQ(rec.data[2], 0xBB);
}

TEST_F(DpHalI2cDeviceTest, ReadReg) {
    dp::hal::I2cDevice<dp::hal::mock::MockI2cBus> dev(i2c_, 0x68);

    const uint8_t staged[] = {0xDD};
    i2c_.stageReadData(0x68, staged, 1);

    uint8_t val = 0;
    EXPECT_EQ(dev.readReg(0x0F, &val, 1), dp::hal::Status::kOk);
    EXPECT_EQ(val, 0xDD);
}

TEST_F(DpHalI2cDeviceTest, WriteRegMaxLength) {
    dp::hal::I2cDevice<dp::hal::mock::MockI2cBus> dev(i2c_, 0x50);

    uint8_t data[32];
    for (size_t i = 0; i < sizeof(data); ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    // 32 bytes is the maximum -- should succeed
    EXPECT_EQ(dev.writeReg(0x00, data, 32), dp::hal::Status::kOk);
    ASSERT_EQ(i2c_.writeRecords().size(), 1u);
    EXPECT_EQ(i2c_.writeRecords()[0].data.size(), 33u);
}

}  // namespace

#endif  // TEST_I2C_CPP_
