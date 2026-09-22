
#ifndef DP_OSAL_MEASUREMENT_CLOCK_H_
#define DP_OSAL_MEASUREMENT_CLOCK_H_
#include <chrono>
#include "detail/dp_osal_measurement_detail.h"
namespace dp::osal::detail {
inline MeasurementStamp measurementNow() noexcept {
    const auto count = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (count < 0 || !std::chrono::steady_clock::is_steady) return {};
    return {static_cast<uint64_t>(count), 1000000U, 1U, 1U, true};
}
inline constexpr bool measurementClockAvailable() noexcept {
    return std::chrono::steady_clock::is_steady;
}
inline constexpr bool measurementThreadIsrTag() noexcept { return true; }
} // namespace dp::osal::detail
#endif
