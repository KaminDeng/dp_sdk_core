//
// Created by kamin.deng on 2024/8/22.
//

#ifndef OSAL_CHRONO_H
#define OSAL_CHRONO_H

#if DP_OSAL_ENABLE_CHRONO

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "interface_chrono.h"

namespace dp::osal {

class Chrono : public ChronoBase<Chrono> {
    friend class ChronoBase<Chrono>;

public:
    static Chrono &getInstance() {
        static Chrono instance;
        return instance;
    }

private:
    TimePoint doNow() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return static_cast<TimePoint>(duration.count());
    }

    Duration doElapsed(const TimePoint &start, const TimePoint &end) const {
        return static_cast<Duration>(end - start) / 1000.0;  // 毫秒转换为秒
    }

    std::time_t doToTimeT(const TimePoint &timePoint) const {
        return static_cast<std::time_t>(timePoint / 1000);  // 毫秒转换为秒
    }

    TimePoint doFromTimeT(std::time_t time) const {
        return static_cast<TimePoint>(time * 1000);  // 秒转换为毫秒
    }

    std::string doToString(const TimePoint &timePoint) const {
        std::time_t time = doToTimeT(timePoint);
        std::tm *tm = std::localtime(&time);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    ~Chrono() = default;

    Chrono() = default;
};

} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_CHRONO */

#endif  // OSAL_CHRONO_H
