//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ISYSTEM_H_
#define ISYSTEM_H_

#include <cstdint> /* uint32_t — required before any platform-specific includes */

namespace osal {

class ISystem {
public:
    virtual void StartScheduler() {}

    // 休眠指定的时间
    virtual void sleep_ms(uint32_t milliseconds) const = 0;

    // 休眠指定的时间
    virtual void sleep(uint32_t seconds) const = 0;

    // 获取系统信息
    virtual const char *get_system_info() const = 0;
};

}  // namespace osal
#endif  // ISYSTEM_H_
