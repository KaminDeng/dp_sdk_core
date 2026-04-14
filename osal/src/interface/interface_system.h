/** @file interface_system.h
 *  @brief CRTP system/scheduler interface for OSAL. */
#ifndef OSAL_INTERFACE_SYSTEM_H_
#define OSAL_INTERFACE_SYSTEM_H_

#include <cstddef>
#include <cstdint>

#include "osal_thread_snapshot.h"

#ifndef OSAL_ENABLE_THREAD_SNAPSHOT
#define OSAL_ENABLE_THREAD_SNAPSHOT 0
#endif

namespace osal {

template <typename Impl>
class SystemBase {
public:
    void StartScheduler() { impl().doStartScheduler(); }
    void sleep_ms(uint32_t milliseconds) const { impl().doSleepMs(milliseconds); }
    void sleep(uint32_t seconds) const { impl().doSleep(seconds); }
    [[nodiscard]] uint32_t get_tick_ms() const { return impl().doGetTickMs(); }
    void enter_critical() const { impl().doEnterCritical(); }
    void exit_critical() const { impl().doExitCritical(); }
    [[nodiscard]] const char *get_system_info() const { return impl().doGetSystemInfo(); }
#if OSAL_ENABLE_THREAD_SNAPSHOT
    size_t get_thread_snapshot(ThreadSnapshot *buf, size_t max) const { return impl().doGetThreadSnapshot(buf, max); }
#endif

protected:
    ~SystemBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace osal

#endif  // OSAL_INTERFACE_SYSTEM_H_
