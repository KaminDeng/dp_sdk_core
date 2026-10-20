/** @brief 旧公开调用及开关兼容；reset 只在本测试没有并发采样者的边界调用。 */
#include "dp_osal_measurement.h"
#include <gtest/gtest.h>
#include <type_traits>
namespace {
using dp::osal::Measurement;
using dp::osal::SynchronizationKind;
TEST(OSALRuntimeMeasurement, PublicResultKeepsAggregateAndStructuredBindingContract) {
    static_assert(std::is_aggregate_v<dp::osal::SynchronizationMeasurement>);
    const dp::osal::SynchronizationMeasurement value{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U,
                                                    10U, 11U, 12U, 13U, 14U, 0U, 0U, 0U, 1U};
    const auto [lc, failed, lt, lm, wc, wt, wm, sc, ic, qc, qt, qm, overhead, sequence,
                valid, overflow, enabled, domain] = value;
    EXPECT_EQ(lc, 1U); EXPECT_EQ(failed, 2U); EXPECT_EQ(lt, 3U); EXPECT_EQ(lm, 4U);
    EXPECT_EQ(wc, 5U); EXPECT_EQ(wt, 6U); EXPECT_EQ(wm, 7U); EXPECT_EQ(sc, 8U);
    EXPECT_EQ(ic, 9U); EXPECT_EQ(qc, 10U); EXPECT_EQ(qt, 11U); EXPECT_EQ(qm, 12U);
    EXPECT_EQ(overhead, 13U); EXPECT_EQ(sequence, 14U); EXPECT_EQ(valid, 0U);
    EXPECT_EQ(overflow, 0U); EXPECT_EQ(enabled, 0U); EXPECT_EQ(domain, 1U);
}
TEST(OSALRuntimeMeasurement, Legacy32BitWaitCannotProveMultipleWraps) {
    Measurement::reset(); Measurement::set_enabled(true);
    Measurement::record_wait(SynchronizationKind::kLock, 0xFFFFFFF0U, 0x00000010U, false);
    const auto value = Measurement::snapshot();
#if DP_OSAL_ENABLE_MEASUREMENT
    EXPECT_EQ(value.lock_wait_count, 1U);
    EXPECT_EQ(value.lock_contended_count, 1U); // 保持失败次数，不能称为真实竞争次数。
#else
    EXPECT_EQ(value.lock_wait_count, 0U);
    EXPECT_FALSE(Measurement::clock_available());
    EXPECT_EQ(Measurement::now_us(), 0U);
    EXPECT_FALSE(Measurement::enabled());
#endif
    EXPECT_EQ(value.lock_wait_total_us, 0U);
    EXPECT_EQ(value.valid, 0U);
}
TEST(OSALRuntimeMeasurement, HostIsrLabelIsOnlyThreadAttribution) {
    Measurement::reset(); Measurement::set_enabled(true);
    Measurement::record_signal(false); Measurement::record_signal(true);
    const auto value = Measurement::snapshot();
#if DP_OSAL_ENABLE_MEASUREMENT
    EXPECT_EQ(value.wake_signal_count, dp::osal::detail::measurementThreadIsrTag() ? 2U : 1U);
    EXPECT_EQ(value.wake_isr_signal_count, dp::osal::detail::measurementThreadIsrTag() ? 1U : 0U);
#else
    EXPECT_EQ(value.wake_signal_count, 0U);
    EXPECT_EQ(value.wake_isr_signal_count, 0U);
#endif
}
TEST(OSALRuntimeMeasurement, DisabledInstrumentationDoesNotCreateSamples) {
    Measurement::reset(); Measurement::set_enabled(false);
    Measurement::record_wait(SynchronizationKind::kQueue, 1U, 2U, true);
    Measurement::record_signal(true);
    const auto value = Measurement::snapshot();
    EXPECT_EQ(value.queue_wait_count, 0U);
    EXPECT_EQ(value.wake_signal_count, 0U);
    EXPECT_EQ(value.valid, 0U);
    Measurement::set_enabled(true);
}
TEST(OSALRuntimeMeasurement, ReenablePreservesStatistics) {
    Measurement::reset(); Measurement::set_enabled(true); Measurement::record_signal(false);
    const auto before = Measurement::snapshot().wake_signal_count;
    Measurement::set_enabled(false); Measurement::record_signal(false);
    EXPECT_EQ(Measurement::snapshot().wake_signal_count, before);
    Measurement::set_enabled(true);
    EXPECT_EQ(Measurement::snapshot().wake_signal_count, before);
}
} // namespace
