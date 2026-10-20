/** @brief 三个独立进程的完整采样路径测试；不链接 dp_osal 归档，避免时钟 ODR 混用。 */
#include "dp_osal_measurement.h"
#include "dp_osal_mutex.h"
#include "dp_osal_queue.h"
#include "dp_osal_semaphore.h"
#include <gtest/gtest.h>
#include <chrono>
#include <limits>
#include <thread>
static uint32_t legacyClockReads = 0U;
extern "C" uint32_t dp_osal_port_measurement_now_us(void) { ++legacyClockReads; return 0xABCDU; }
// header-only 真实原语的诊断出口；这些测试不装配核心归档。
namespace dp::osal {
void DP_OSAL_LOGE_(const char*, const char*, int, const char*, ...) {}
void DP_OSAL_LOGD_(const char*, const char*, int, const char*, ...) {}
}
namespace {
using namespace dp::osal;
using detail::MeasurementStamp;
class OSALMeasurementClock : public ::testing::Test {
protected:
    void SetUp() override { Measurement::set_enabled(true); Measurement::reset(); }
};
// 确定地把开关切换放在业务后端内部，不依赖线程调度或睡眠。
class SignalSwitchProbe : public SemaphoreBase<SignalSwitchProbe> {
    friend class SemaphoreBase<SignalSwitchProbe>;
public:
    bool enabled_after_signal = true;
    uint32_t signal_calls = 0U;
    uint32_t isr_calls = 0U;
private:
    void doSignal() { ++signal_calls; Measurement::set_enabled(enabled_after_signal); }
    void doSignalFromIsr() { ++isr_calls; Measurement::set_enabled(enabled_after_signal); }
};
TEST_F(OSALMeasurementClock, SignalDisabledAtEntryDoesNotSampleWhenBackendEnables) {
    SignalSwitchProbe probe;
    Measurement::set_enabled(false);
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    const auto reads = detail::measurementClockReads;
#endif
    probe.signal();
    EXPECT_EQ(probe.signal_calls, 1U);
    EXPECT_EQ(Measurement::snapshot().wake_signal_count, 0U);
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    EXPECT_EQ(detail::measurementClockReads, reads);
#endif
    // 后续事件入口已启用才可采样，开启不追补之前的事件。
    probe.signal();
    EXPECT_EQ(probe.signal_calls, 2U);
    EXPECT_EQ(Measurement::snapshot().wake_signal_count, 1U);
}
TEST_F(OSALMeasurementClock, HostIsrTagDisabledAtEntryDoesNotSampleWhenBackendEnables) {
    SignalSwitchProbe probe;
    Measurement::set_enabled(false);
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    const auto reads = detail::measurementClockReads;
#endif
    probe.signalFromIsr();
    EXPECT_EQ(probe.isr_calls, 1U);
    EXPECT_EQ(Measurement::snapshot().wake_signal_count, 0U);
    EXPECT_EQ(Measurement::snapshot().wake_isr_signal_count, 0U);
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    EXPECT_EQ(detail::measurementClockReads, reads);
#endif
    probe.signalFromIsr();
    EXPECT_EQ(probe.isr_calls, 2U);
    const auto expected = detail::measurementThreadIsrTag() ? 1U : 0U;
    EXPECT_EQ(Measurement::snapshot().wake_signal_count, expected);
    EXPECT_EQ(Measurement::snapshot().wake_isr_signal_count, expected);
}
TEST_F(OSALMeasurementClock, SignalEnabledAtEntryIsDiscardedWhenBackendDisables) {
    SignalSwitchProbe probe; probe.enabled_after_signal = false;
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    const auto reads = detail::measurementClockReads;
#endif
    probe.signal();
    EXPECT_EQ(probe.signal_calls, 1U);
    EXPECT_EQ(Measurement::snapshot().wake_signal_count, 0U);
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    EXPECT_EQ(detail::measurementClockReads, reads);
#endif
}
TEST_F(OSALMeasurementClock, HostIsrTagEnabledAtEntryIsDiscardedWhenBackendDisables) {
    SignalSwitchProbe probe; probe.enabled_after_signal = false;
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    const auto reads = detail::measurementClockReads;
#endif
    probe.signalFromIsr();
    EXPECT_EQ(probe.isr_calls, 1U);
    EXPECT_EQ(Measurement::snapshot().wake_signal_count, 0U);
    EXPECT_EQ(Measurement::snapshot().wake_isr_signal_count, 0U);
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    EXPECT_EQ(detail::measurementClockReads, reads);
#endif
}
TEST_F(OSALMeasurementClock, LegacyShortClockRemainsAvailableWithoutQualifyingWideSamples) {
    const auto reads = legacyClockReads;
    EXPECT_EQ(Measurement::now_us(), 0xABCDU);
    EXPECT_EQ(legacyClockReads, reads + 1U);
    EXPECT_EQ(Measurement::clock_available(), detail::measurementClockAvailable());
    EXPECT_EQ(Measurement::snapshot().valid,
              detail::measurementClockAvailable() && detail::measurementThreadIsrTag() ? 1U : 0U);
}
TEST_F(OSALMeasurementClock, RealPrimitivesKeepBusinessResultsAndCounts) {
    Mutex mutex;
    ASSERT_TRUE(mutex.lock()); ASSERT_TRUE(mutex.unlock());
    ASSERT_TRUE(mutex.tryLock()); ASSERT_TRUE(mutex.unlock());
    ASSERT_TRUE(mutex.tryLockFor(1U)); ASSERT_TRUE(mutex.unlock());
    MessageQueue<int> queue;
    queue.send(42); EXPECT_EQ(queue.receive(), 42);
    ASSERT_TRUE(queue.trySend(7)); int output = 0;
    ASSERT_TRUE(queue.tryReceive(output)); EXPECT_EQ(output, 7);
    EXPECT_FALSE(queue.receiveFor(output, 1U));
    Semaphore semaphore;
    semaphore.signal(); semaphore.wait();
    semaphore.signalFromIsr(); EXPECT_TRUE(semaphore.tryWait());
    EXPECT_FALSE(semaphore.tryWaitFor(1U));
    const auto value = Measurement::snapshot();
    EXPECT_EQ(value.lock_wait_count, 3U); EXPECT_EQ(value.queue_wait_count, 5U);
    EXPECT_EQ(value.wake_wait_count, 3U);
    EXPECT_EQ(value.wake_signal_count, detail::measurementThreadIsrTag() ? 2U : 1U);
    EXPECT_EQ(value.valid, detail::measurementClockAvailable() && detail::measurementThreadIsrTag() ? 1U : 0U);
}
TEST_F(OSALMeasurementClock, WideWaitAccumulatesAndLegacyWaitInvalidates) {
    const MeasurementStamp begin{10U, 1000000U, 1U, 1U, true};
    const MeasurementStamp end{30U, 1000000U, 1U, 1U, true};
    detail::MeasurementAccess::recordWait(SynchronizationKind::kLock, begin, end, false);
    auto value = Measurement::snapshot();
    EXPECT_EQ(value.lock_wait_total_us, 20U); EXPECT_EQ(value.lock_wait_max_us, 20U);
    EXPECT_EQ(value.lock_contended_count, 1U);
    Measurement::record_wait(SynchronizationKind::kLock, 10U, 30U, true);
    value = Measurement::snapshot(); EXPECT_EQ(value.lock_wait_count, 2U); EXPECT_EQ(value.valid, 0U);
}
TEST_F(OSALMeasurementClock, MultiWrapDurationAndAccumulationOverflowInvalidateWholeResult) {
    const MeasurementStamp begin{10U, 1000000U, 1U, 1U, true};
    auto end = begin; end.us += uint64_t{1} << 33U;
    detail::MeasurementAccess::recordWait(SynchronizationKind::kWake, begin, end, true);
    auto value = Measurement::snapshot();
    EXPECT_EQ(value.wake_wait_count, 1U); EXPECT_EQ(value.overflow, 1U); EXPECT_EQ(value.valid, 0U);
    EXPECT_EQ(value.wake_wait_total_us, 0U);
    Measurement::reset(); end.us = begin.us + std::numeric_limits<uint32_t>::max();
    detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, begin, end, true);
    end.us = begin.us + 1U;
    detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, begin, end, true);
    value = Measurement::snapshot();
    EXPECT_EQ(value.queue_wait_total_us, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(value.queue_wait_count, 2U); EXPECT_EQ(value.overflow, 1U); EXPECT_EQ(value.valid, 0U);
}
TEST_F(OSALMeasurementClock, EveryTimeQualityFailureInvalidatesWholeSnapshot) {
    const MeasurementStamp begin{10U, 1000000U, 1U, 1U, true};
    for (unsigned reason = 0; reason < 6; ++reason) {
        Measurement::reset(); auto end = begin; end.us = 20U;
        switch (reason) {
        case 0: end.valid = false; break;
        case 1: end.frequency = 0U; break;
        case 2: end.frequency = 500000U; break;
        case 3: end.epoch = 2U; break;
        case 4: end.domain = 2U; break;
        default: end.us = 9U; break;
        }
        detail::MeasurementAccess::recordWait(SynchronizationKind::kLock, begin, end, true);
        const auto value = Measurement::snapshot();
        EXPECT_EQ(value.lock_wait_count, 1U); EXPECT_EQ(value.lock_wait_total_us, 0U);
        EXPECT_EQ(value.valid, 0U);
    }
}
TEST_F(OSALMeasurementClock, DisabledRealPrimitivesKeepWaitAndReturnValues) {
    Measurement::set_enabled(false); // 本测试没有在途事件；不能推广成关闭即屏障。
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    const auto reads = detail::measurementClockReads;
#endif
    const auto legacy_reads = legacyClockReads;
    Mutex mutex; ASSERT_TRUE(mutex.lock()); ASSERT_TRUE(mutex.unlock());
    ASSERT_TRUE(mutex.tryLock()); ASSERT_TRUE(mutex.unlock());
    ASSERT_TRUE(mutex.tryLockFor(1U)); ASSERT_TRUE(mutex.unlock());
    MessageQueue<int> queue; queue.send(8); EXPECT_EQ(queue.receive(), 8);
    EXPECT_TRUE(queue.trySend(9)); int out = 0; EXPECT_TRUE(queue.tryReceive(out));
    EXPECT_EQ(out, 9); EXPECT_FALSE(queue.receiveFor(out, 1U));
    Semaphore semaphore; semaphore.signal(); semaphore.wait();
    semaphore.signalFromIsr(); EXPECT_TRUE(semaphore.tryWait());
    const auto begin = std::chrono::steady_clock::now();
    EXPECT_FALSE(semaphore.tryWaitFor(5U));
    EXPECT_GE(std::chrono::steady_clock::now() - begin, std::chrono::milliseconds(4));
    Measurement::record_signal(false); Measurement::record_instrumentation(1U, 2U);
    EXPECT_EQ(Measurement::snapshot().sample_sequence, 0U);
    EXPECT_EQ(legacyClockReads, legacy_reads);
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
    EXPECT_EQ(detail::measurementClockReads, reads);
#endif
}
#if defined(DP_OSAL_TEST_MEASUREMENT_FIXTURE)
TEST_F(OSALMeasurementClock, FixtureCountsEntireSamplingPathAndUnsupportedIsrDoesNotRead) {
    Mutex mutex; const auto before = detail::measurementClockReads;
    ASSERT_TRUE(mutex.lock()); ASSERT_TRUE(mutex.unlock());
    EXPECT_EQ(detail::measurementClockReads - before, 4U);
    Semaphore semaphore; const auto isr_before = detail::measurementClockReads;
    semaphore.signalFromIsr();
    EXPECT_EQ(detail::measurementClockReads - isr_before, detail::measurementThreadIsrTag() ? 2U : 0U);
    EXPECT_EQ(Measurement::snapshot().valid, detail::measurementThreadIsrTag() ? 1U : 0U);
}
#endif
} // namespace
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
