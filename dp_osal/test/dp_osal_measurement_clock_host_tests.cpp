#include "dp_osal_measurement.h"
#include <cassert>

#if defined(DP_OSAL_TEST_CLOCK_HOOK)
extern "C" uint32_t dp_osal_port_measurement_now_us(void) {
    static uint32_t ticks = 1000U;
    ticks += 10U;
    return ticks;
}
#endif
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
