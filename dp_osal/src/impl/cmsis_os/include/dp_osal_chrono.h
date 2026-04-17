//
// Created by kamin.deng on 2024/8/22.
//

#ifndef OSAL_CHRONO_H
#define OSAL_CHRONO_H

#if DP_OSAL_ENABLE_CHRONO

#include <cstdio>  // for std::sprintf
#include <ctime>   // for std::tm and std::time_t

#include "interface_chrono.h"
#include "dp_osal_port.h"

namespace dp::osal {

class Chrono : public ChronoBase<Chrono> {
    friend class ChronoBase<Chrono>;

public:
    static Chrono &getInstance() {
        static Chrono instance;
        return instance;
    }

private:
    TimePoint doNow() const { return osKernelGetTickCount(); }

    Duration doElapsed(const TimePoint &start, const TimePoint &end) const {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<Duration>(end - start) / tickFreq;
    }

    std::time_t doToTimeT(const TimePoint &timePoint) const {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<std::time_t>(timePoint / tickFreq);
    }

    TimePoint doFromTimeT(std::time_t time) const {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<TimePoint>(time * tickFreq);
    }

    std::string doToString(const TimePoint &timePoint) const {
        std::time_t time = doToTimeT(timePoint);
        std::tm *tm = std::localtime(&time);

        char buffer[100];  // 假设你已经定义了这个缓冲区，并且大小足够
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1,
                      tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

        return std::string(buffer);
    }

    ~Chrono() = default;

    Chrono() = default;
};
} // namespace dp::osal

#endif /* DP_OSAL_ENABLE_CHRONO */

#endif  // OSAL_CHRONO_H
