/** @file interface_system.h
 *  @brief CRTP system/scheduler interface for OSAL. */
#ifndef OSAL_INTERFACE_SYSTEM_H_
#define OSAL_INTERFACE_SYSTEM_H_

#include <cstddef>
#include <cstdint>

#include "osal_task_snapshot.h"

#ifndef OSAL_ENABLE_TASK_SNAPSHOT
#define OSAL_ENABLE_TASK_SNAPSHOT 0
#endif

namespace osal {

template <typename Impl>
class SystemBase {
public:
    void StartScheduler() { impl().doStartScheduler(); }
    void sleep_ms(uint32_t milliseconds) const { impl().doSleepMs(milliseconds); }
    void sleep(uint32_t seconds) const { impl().doSleep(seconds); }
    [[nodiscard]] const char *get_system_info() const { return impl().doGetSystemInfo(); }
#if OSAL_ENABLE_TASK_SNAPSHOT
    size_t get_task_snapshot(TaskSnapshot *buf, size_t max) const { return impl().doGetTaskSnapshot(buf, max); }
#endif

protected:
    ~SystemBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace osal

#endif  // OSAL_INTERFACE_SYSTEM_H_
