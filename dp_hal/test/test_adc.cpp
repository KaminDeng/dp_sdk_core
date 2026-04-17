/** @file   test_adc.cpp
 *  @brief  GTest suite for dp_hal ADC CRTP interface with MockAdc. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_ADC_CPP_
#define TEST_ADC_CPP_

#include <gtest/gtest.h>

#include "dp_adc.h"
#include "mock_adc.h"

namespace {

class DpHalAdcTest : public ::testing::Test {
protected:
    void SetUp() override { adc_.reset(); }

    dp::hal::mock::MockAdc adc_;
};

TEST_F(DpHalAdcTest, ConfigureRecordsParameters) {
    EXPECT_EQ(adc_.configure(0, 12), dp::hal::Status::kOk);
    ASSERT_EQ(adc_.configRecords().size(), 1u);
    EXPECT_EQ(adc_.configRecords()[0].channel, 0u);
    EXPECT_EQ(adc_.configRecords()[0].resolution_bits, 12u);
}

TEST_F(DpHalAdcTest, ReadReturnsStagedValue) {
    adc_.stageChannelValue(3, 2048);
    EXPECT_EQ(adc_.read(3), 2048u);
    ASSERT_EQ(adc_.readChannels().size(), 1u);
    EXPECT_EQ(adc_.readChannels()[0], 3u);
}

TEST_F(DpHalAdcTest, ReadUnstagedReturnsZero) { EXPECT_EQ(adc_.read(7), 0u); }

TEST_F(DpHalAdcTest, ContinuousNotSupported) {
    EXPECT_EQ(adc_.startContinuous(0, nullptr, nullptr), dp::hal::Status::kNotSupported);
    EXPECT_EQ(adc_.stopContinuous(0), dp::hal::Status::kNotSupported);
}

TEST_F(DpHalAdcTest, MultipleChannels) {
    adc_.stageChannelValue(0, 100);
    adc_.stageChannelValue(1, 200);
    adc_.stageChannelValue(2, 300);

    EXPECT_EQ(adc_.read(2), 300u);
    EXPECT_EQ(adc_.read(0), 100u);
    EXPECT_EQ(adc_.read(1), 200u);
    EXPECT_EQ(adc_.readChannels().size(), 3u);
}

}  // namespace

#endif  // TEST_ADC_CPP_
