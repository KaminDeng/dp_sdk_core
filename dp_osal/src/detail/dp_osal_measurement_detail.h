/** @file dp_osal_measurement_detail.h
 * @brief 私有宽时间戳及时间域质量检查，不包含时间源或采集状态。 */
#ifndef DP_OSAL_MEASUREMENT_DETAIL_H_
#define DP_OSAL_MEASUREMENT_DETAIL_H_
#include <cstdint>
namespace dp::osal::detail {
struct MeasurementStamp {
    uint64_t us;
    uint32_t frequency;
    uint32_t epoch;
    uint8_t domain;
    bool valid;
};
inline bool elapsedUs(const MeasurementStamp& begin, const MeasurementStamp& end,
                      uint64_t& result) noexcept {
    if (!begin.valid || !end.valid || begin.frequency == 0U ||
        begin.frequency != end.frequency || begin.epoch != end.epoch ||
        begin.domain != end.domain || end.us < begin.us) return false;
    result = end.us - begin.us;
    return true;
}
struct MeasurementAccess;
} // namespace dp::osal::detail
#endif
