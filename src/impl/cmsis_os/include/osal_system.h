//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include "interface_system.h"
#include "osal.h"

namespace osal {

class OSALSystem : public ISystem {
public:
    static OSALSystem &getInstance() {
        static OSALSystem instance;
        return instance;
    }

    void StartScheduler() override {
        osKernelInitialize();
        osKernelStart();
    }

    void sleep_ms(const uint32_t milliseconds) const override { osDelay(milliseconds); }

    void sleep(const uint32_t seconds) const override { osDelay(seconds * 1000); }

    [[nodiscard]] const char *get_system_info() const override {
        // Return some basic system information
        return "CMSIS-RTOS2 System";
    }

private:
    OSALSystem() {};

    ~OSALSystem() {};
};

}  // namespace osal
#endif  // OSAL_SYSTEM_H_
