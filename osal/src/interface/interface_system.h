/** @file interface_system.h
 *  @brief CRTP system/scheduler interface for OSAL. */
#ifndef OSAL_INTERFACE_SYSTEM_H_
#define OSAL_INTERFACE_SYSTEM_H_

#include <cstdint>

namespace osal {

template <typename Impl>
class SystemBase {
public:
    void StartScheduler() { impl().doStartScheduler(); }
    void sleep_ms(uint32_t milliseconds) const { impl().doSleepMs(milliseconds); }
    void sleep(uint32_t seconds) const { impl().doSleep(seconds); }
    [[nodiscard]] const char *get_system_info() const { return impl().doGetSystemInfo(); }

protected:
    ~SystemBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace osal

#endif  // OSAL_INTERFACE_SYSTEM_H_
