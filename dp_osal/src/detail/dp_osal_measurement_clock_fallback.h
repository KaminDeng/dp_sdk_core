
#ifndef DP_OSAL_MEASUREMENT_CLOCK_FALLBACK_H_
#define DP_OSAL_MEASUREMENT_CLOCK_FALLBACK_H_
#include "detail/dp_osal_measurement_detail.h"
namespace dp::osal::detail {
inline MeasurementStamp measurementNow() noexcept { return {}; }
inline constexpr bool measurementClockAvailable() noexcept { return false; }
inline constexpr bool measurementThreadIsrTag() noexcept { return false; }
} // namespace dp::osal::detail
#endif
