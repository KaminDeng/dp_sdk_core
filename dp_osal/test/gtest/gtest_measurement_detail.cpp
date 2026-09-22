
#include "detail/dp_osal_measurement_detail.h"
#include <gtest/gtest.h>
namespace {
using dp::osal::detail::MeasurementStamp;
using dp::osal::detail::elapsedUs;
TEST(OSALMeasurementDetail, EpochAndLongDuration) {
    uint64_t result = 0U;
    const MeasurementStamp begin{10U, 1000000U, 1U, 1U, true};
    MeasurementStamp end{(uint64_t{1} << 33U) + 10U, 1000000U, 1U, 1U, true};
    ASSERT_TRUE(elapsedUs(begin, end, result)); EXPECT_EQ(result, uint64_t{1} << 33U);
    end.epoch = 2U; EXPECT_FALSE(elapsedUs(begin, end, result));
}
TEST(OSALMeasurementDetail, MissingSourceFrequencyDomainAndBackwardsAreInvalid) {
    const MeasurementStamp begin{10U, 1000000U, 1U, 1U, true};
    uint64_t result = 123U;
    auto end = begin; end.valid = false; EXPECT_FALSE(elapsedUs(begin, end, result));
    end = begin; end.frequency = 0U; EXPECT_FALSE(elapsedUs(begin, end, result));
    end.frequency = 500000U; EXPECT_FALSE(elapsedUs(begin, end, result));
    end = begin; end.domain = 2U; EXPECT_FALSE(elapsedUs(begin, end, result));
    end = begin; end.us = 9U; EXPECT_FALSE(elapsedUs(begin, end, result));
    auto invalid_begin = begin; invalid_begin.valid = false;
    EXPECT_FALSE(elapsedUs(invalid_begin, begin, result));
    EXPECT_EQ(result, 123U); // 拒绝时不发布伪造差值。
}
TEST(OSALMeasurementDetail, ReturningFrequencyStillRequiresNewEpoch) {
    const MeasurementStamp begin{10U, 1000000U, 1U, 1U, true};
    const MeasurementStamp end{20U, 1000000U, 3U, 1U, true};
    uint64_t result = 0U; EXPECT_FALSE(elapsedUs(begin, end, result));
}
} // namespace
