/** @file interface_chrono.h
 *  @brief Abstract interface for OSAL time/clock services. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ICHRONO_H_
#define ICHRONO_H_

#if OSAL_ENABLE_CHRONO

#include <string>

namespace osal {

/** @brief Abstract clock and time-conversion interface.
 *
 *  Provides a portable way to read the current time, compute elapsed
 *  durations, and convert between the OSAL @c TimePoint representation
 *  and standard C/C++ time types.  Concrete backends map these operations
 *  to the underlying RTOS tick counter or POSIX clock. */
class IChrono {
public:
    // 定义具体的类型
    using TimePoint = uint32_t;  ///< Kernel tick count used as a time point.
    using Duration = double;     ///< Time interval expressed in seconds.

    /** @brief Returns the current time point (tick count).
     *  @return Current kernel tick count. */
    [[nodiscard]] virtual TimePoint now() const = 0;

    /** @brief Computes the elapsed time between two time points.
     *  @param start  Earlier time point.
     *  @param end    Later time point.
     *  @return Elapsed time in seconds. */
    [[nodiscard]] virtual Duration elapsed(const TimePoint &start, const TimePoint &end) const = 0;

    /** @brief Converts a time point to a @c std::time_t wall-clock value.
     *  @param timePoint  Time point to convert.
     *  @return Corresponding @c std::time_t value. */
    [[nodiscard]] virtual std::time_t to_time_t(const TimePoint &timePoint) const = 0;

    /** @brief Converts a @c std::time_t wall-clock value to a time point.
     *  @param time  Wall-clock value to convert.
     *  @return Corresponding @c TimePoint. */
    [[nodiscard]] virtual TimePoint from_time_t(std::time_t time) const = 0;

    /** @brief Formats a time point as a human-readable string.
     *  @param timePoint  Time point to format.
     *  @return Formatted string representation. */
    [[nodiscard]] virtual std::string to_string(const TimePoint &timePoint) const = 0;
};

}  // namespace osal

#endif /* OSAL_ENABLE_CHRONO */

#endif  // ICHRONO_H_
