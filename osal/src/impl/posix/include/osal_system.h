//
// Created by kamin.deng on 2024/8/23.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>

#include "interface_system.h"
#include "osal_thread.h"  // for osal_sleep_ms_interruptible() + tl_stop_ctx

namespace osal {

class OSALSystem : public SystemBase<OSALSystem> {
    friend class SystemBase<OSALSystem>;

public:
    static OSALSystem &getInstance() {
        static OSALSystem instance;
        return instance;
    }

private:
    void doStartScheduler() {
        // POSIX 系统不需要显式启动调度器
        while (1)
            ;
    }

    void doSleepMs(const uint32_t milliseconds) const { osal_sleep_ms_interruptible(milliseconds); }

    void doSleep(const uint32_t seconds) const { std::this_thread::sleep_for(std::chrono::seconds(seconds)); }

    const char *doGetSystemInfo() const {
        // 返回一些基本的系统信息
        return "POSIX System";
    }
    OSALSystem(){};

    ~OSALSystem(){};
};

}  // namespace osal
#endif  // OSAL_SYSTEM_H_
