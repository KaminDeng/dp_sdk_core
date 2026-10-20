/**
 * @file gtest_runtime_measurement.cpp
 * @brief 直接向收集器注入事件，验证模微秒差、ISR归属计数与关闭插桩分支。
 * @details 不是互斥锁/信号量实际阻塞测量，也没有真实ISR；计时输入由用例手工提供。
 * 全局收集器被reset/切换，运行时需与其它测量使用者隔离，避免统计污染。
 */
#include "dp_osal_measurement.h"

#include <gtest/gtest.h>

namespace {

using dp::osal::Measurement;
using dp::osal::SynchronizationKind;

/**
 * @brief 注入FFFFFFF0→00000010的锁事件，要求次数1、总/最大耗时均32微秒且valid。
 * @details 只检查一次模差，valid还依赖测试装配时钟入口，不覆盖多个完整回绕。
 */
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

/**
 * @brief 注入普通与ISR标记各一次，核发送总数2、ISR子集1及样本次数。
 * @details true标记来自测试调用，不代表执行过真实中断上下文。
 */
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

/**
 * @brief 关闭后注入队列事件与ISR发送，要求计数不增、valid=0，末尾恢复开启。
 * @note EXPECT非fatal保证通常走到恢复，但测试异常退出不由本文件专门清理。
 */
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
