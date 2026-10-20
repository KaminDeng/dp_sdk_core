/**
 * @file dp_osal_measurement_clock_host_tests.cpp
 * @brief 用编译宏选择主机、无时钟MCU模拟及固定步进hook，检查测量有效性。
 * @details 不读取真实MCU计数器；DPSDK_MCU_FIRMWARE在此只选择头文件分支。
 * 使用标准assert表达预期，必须在未定义NDEBUG的测试装配中执行才有断言检查。
 */
#include "dp_osal_measurement.h"
#include <cassert>

#if defined(DP_OSAL_TEST_CLOCK_HOOK)
/**
 * @brief 测试时钟hook：每调用一次固定增加10微秒，从1000开始。
 * @return 32位模拟计数，不读取硬件或系统真实时间。
 * @note 仅DP_OSAL_TEST_CLOCK_HOOK目标提供此定义，静态局部计数没有多线程同步。
 */
extern "C" uint32_t dp_osal_port_measurement_now_us(void) {
    static uint32_t ticks = 1000U;
    ticks += 10U;
    return ticks;
}
#endif
/**
 * @brief 重设收集器，记录一次锁等待并核对三种时钟配置的valid/时间差，最后关闭插桩。
 * @return 正常结束隐式返回0；assert不满足会终止，NDEBUG会移除这些检查。
 * @details 无hook MCU模拟仍记录一次调用但资格无效且耗时零；固定hook配置差值为10微秒；
 * 主机回退要求时钟入口可用，最后确认关闭插桩后valid=0。
 */
int main() {
    using dp::osal::Measurement;
    Measurement::reset(); Measurement::set_enabled(true);
    const auto before = Measurement::now_us();
    Measurement::record_wait(dp::osal::SynchronizationKind::kLock, before, Measurement::now_us(), true);
    const auto value = Measurement::snapshot();
    assert(value.lock_wait_count == 1U); // 时钟不可用只影响资格，不改原计数。
#if defined(DPSDK_MCU_FIRMWARE) && !defined(DP_OSAL_TEST_CLOCK_HOOK)
    assert(!Measurement::clock_available());
    assert(value.valid == 0U && value.lock_wait_total_us == 0U);
#else
    assert(Measurement::clock_available() && value.valid == 1U);
#if defined(DP_OSAL_TEST_CLOCK_HOOK)
    assert(value.lock_wait_total_us == 10U);
#endif
#endif
    Measurement::set_enabled(false);
    assert(Measurement::snapshot().valid == 0U);
}
