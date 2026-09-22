#include "dp_osal_measurement.h"

#include <gtest/gtest.h>

namespace {

using dp::osal::Measurement;
using dp::osal::SynchronizationKind;

TEST(OSALRuntimeMeasurement, CounterWrapIsMeasuredModulo32Bits) {
    Measurement::reset();
    Measurement::set_enabled(true);
    Measurement::record_wait(SynchronizationKind::kLock, 0xFFFFFFF0U, 0x00000010U, true);

    const auto snapshot = Measurement::snapshot();
    EXPECT_EQ(snapshot.lock_wait_count, 1U);
    EXPECT_EQ(snapshot.lock_wait_total_us, 0x20U);
    EXPECT_EQ(snapshot.lock_wait_max_us, 0x20U);
    EXPECT_EQ(snapshot.valid, 1U);
}

TEST(OSALRuntimeMeasurement, WakeSignalsKeepIsrAttributionSeparate) {
    Measurement::reset();
    Measurement::set_enabled(true);
    Measurement::record_signal(false);
    Measurement::record_signal(true);

    const auto snapshot = Measurement::snapshot();
    EXPECT_EQ(snapshot.wake_signal_count, 2U);
    EXPECT_EQ(snapshot.wake_isr_signal_count, 1U);
    EXPECT_GE(snapshot.sample_sequence, 2U);
}

TEST(OSALRuntimeMeasurement, DisabledInstrumentationDoesNotCreateSamples) {
    Measurement::reset();
    Measurement::set_enabled(false);
    Measurement::record_wait(SynchronizationKind::kQueue, 1U, 2U, true);
    Measurement::record_signal(true);

    const auto snapshot = Measurement::snapshot();
    EXPECT_EQ(snapshot.queue_wait_count, 0U);
    EXPECT_EQ(snapshot.wake_signal_count, 0U);
    EXPECT_EQ(snapshot.valid, 0U);
    Measurement::set_enabled(true);
}

}  // namespace
