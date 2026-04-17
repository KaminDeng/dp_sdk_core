/** @file interface_chrono.h
 *  @brief CRTP interface for OSAL time/clock services. */
#ifndef DP_OSAL_INTERFACE_CHRONO_H_
#define DP_OSAL_INTERFACE_CHRONO_H_

#include <cstdint>
#include <ctime>
#include <string>

#if DP_OSAL_ENABLE_CHRONO

namespace dp::osal {

template <typename Impl>
class ChronoBase {
public:
    using TimePoint = uint32_t;
    using Duration = double;

    [[nodiscard]] TimePoint now() const { return impl().doNow(); }
    [[nodiscard]] Duration elapsed(const TimePoint &start, const TimePoint &end) const {
        return impl().doElapsed(start, end);
    }
    [[nodiscard]] std::time_t to_time_t(const TimePoint &timePoint) const { return impl().doToTimeT(timePoint); }
    [[nodiscard]] TimePoint from_time_t(std::time_t time) const { return impl().doFromTimeT(time); }
    [[nodiscard]] std::string to_string(const TimePoint &timePoint) const { return impl().doToString(timePoint); }

protected:
    ~ChronoBase() = default;

private:
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_CHRONO */

#endif  // DP_OSAL_INTERFACE_CHRONO_H_
