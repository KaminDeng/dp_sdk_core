//
// Created by kamin.deng on 2024/8/22.
//

#ifndef OSAL_CHRONO_H
#define OSAL_CHRONO_H

#if OSAL_ENABLE_CHRONO

#include <cstdio>  // for std::sprintf
#include <ctime>   // for std::tm and std::time_t

#include "interface_chrono.h"
#include "osal.h"

namespace osal {

class OSALChrono : public IChrono {
public:
    static OSALChrono &getInstance() {
        static OSALChrono instance;
        return instance;
    }

    // 获取当前时间点
    TimePoint now() const override { return osKernelGetTickCount(); }

    // 计算两个时间点之间的时间间隔
    Duration elapsed(const TimePoint &start, const TimePoint &end) const override {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<Duration>(end - start) / tickFreq;
    }

    // 将时间点转换为time_t类型
    std::time_t to_time_t(const TimePoint &timePoint) const override {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<std::time_t>(timePoint / tickFreq);
    }

    // 将time_t类型转换为时间点
    TimePoint from_time_t(std::time_t time) const override {
        uint32_t tickFreq = osKernelGetTickFreq();
        return static_cast<TimePoint>(time * tickFreq);
    }

    // 将时间点转换为字符串
    std::string to_string(const TimePoint &timePoint) const override {
        std::time_t time = to_time_t(timePoint);
        std::tm *tm = std::localtime(&time);

        char buffer[100];  // 假设你已经定义了这个缓冲区，并且大小足够
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1,
                      tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

        return std::string(buffer);
    }

private:
    virtual ~OSALChrono() {}

    OSALChrono() {}
};
}  // namespace osal

#endif /* OSAL_ENABLE_CHRONO */

#endif  // OSAL_CHRONO_H
