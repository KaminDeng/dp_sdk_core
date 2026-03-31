/** @file   test_dac.cpp
 *  @brief  GTest suite for dp_hal DAC CRTP interface with MockDac. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_DAC_CPP_
#define TEST_DAC_CPP_

#include <gtest/gtest.h>

#include "dp_dac.h"
#include "mock_dac.h"

namespace {

class DpHalDacTest : public ::testing::Test {
protected:
    void SetUp() override { dac_.reset(); }

    dp::hal::mock::MockDac dac_;
};

TEST_F(DpHalDacTest, ConfigureRecordsParameters) {
    EXPECT_EQ(dac_.configure(0, 12), dp::hal::Status::kOk);
    ASSERT_EQ(dac_.configRecords().size(), 1u);
    EXPECT_EQ(dac_.configRecords()[0].channel, 0u);
    EXPECT_EQ(dac_.configRecords()[0].resolution_bits, 12u);
}

TEST_F(DpHalDacTest, WriteRecordsValue) {
    EXPECT_EQ(dac_.write(1, 4095), dp::hal::Status::kOk);
    ASSERT_EQ(dac_.writeRecords().size(), 1u);
    EXPECT_EQ(dac_.writeRecords()[0].channel, 1u);
    EXPECT_EQ(dac_.writeRecords()[0].value, 4095u);
}

TEST_F(DpHalDacTest, MultipleWrites) {
    dac_.write(0, 100);
    dac_.write(0, 200);
    dac_.write(1, 300);

    ASSERT_EQ(dac_.writeRecords().size(), 3u);
    EXPECT_EQ(dac_.writeRecords()[0].value, 100u);
    EXPECT_EQ(dac_.writeRecords()[1].value, 200u);
    EXPECT_EQ(dac_.writeRecords()[2].channel, 1u);
}

TEST_F(DpHalDacTest, ResetClearsState) {
    dac_.write(0, 100);
    dac_.configure(0, 12);
    dac_.reset();

    EXPECT_TRUE(dac_.writeRecords().empty());
    EXPECT_TRUE(dac_.configRecords().empty());
}

}  // namespace

#endif  // TEST_DAC_CPP_
