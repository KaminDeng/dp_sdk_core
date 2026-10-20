/** @brief 软件时钟反例夹具，不代表 RTOS 或物理 ISR 能力。 */
#ifndef DP_OSAL_TEST_MEASUREMENT_CLOCK_H_
#define DP_OSAL_TEST_MEASUREMENT_CLOCK_H_
#define DP_OSAL_TEST_MEASUREMENT_FIXTURE 1
#include "detail/dp_osal_measurement_detail.h"
namespace dp::osal::detail {
inline uint32_t measurementClockReads = 0U;
inline MeasurementStamp measurementClockValue{1000U, 1000000U, 1U, 1U, true};
inline MeasurementStamp measurementNow() noexcept {
    ++measurementClockReads;
    measurementClockValue.us += 10U;
    return measurementClockValue;
}
inline constexpr bool measurementClockAvailable() noexcept { return true; }
inline constexpr bool measurementThreadIsrTag() noexcept { return true; }
} // namespace dp::osal::detail
#endif
