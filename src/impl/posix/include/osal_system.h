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

class OSALSystem : public ISystem {
public:
    static OSALSystem &getInstance() {
        static OSALSystem instance;
        return instance;
    }

    void StartScheduler() override {
        // POSIX 系统不需要显式启动调度器
        while (1);
    }

    void sleep_ms(const uint32_t milliseconds) const override { osal_sleep_ms_interruptible(milliseconds); }

    void sleep(const uint32_t seconds) const override { std::this_thread::sleep_for(std::chrono::seconds(seconds)); }

    const char *get_system_info() const override {
        // 返回一些基本的系统信息
        return "POSIX System";
    }

private:
    OSALSystem() {};

    ~OSALSystem() {};
};

}  // namespace osal
#endif  // OSAL_SYSTEM_H_
