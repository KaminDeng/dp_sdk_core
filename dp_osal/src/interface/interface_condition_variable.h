/** @file interface_condition_variable.h
 *  @brief CRTP condition-variable interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_CONDITION_VARIABLE_H_
#define DP_OSAL_INTERFACE_CONDITION_VARIABLE_H_

#include <cstdint>

#if OSAL_ENABLE_CONDITION_VAR

#include "osal_mutex.h"

namespace dp::osal {

template <typename Impl>
class ConditionVariableBase {
public:
    void wait(Mutex &mutex) { impl().doWait(mutex); }
    bool waitFor(Mutex &mutex, uint32_t timeout) { return impl().doWaitFor(mutex, timeout); }
    void notifyOne() { impl().doNotifyOne(); }
    void notifyAll() { impl().doNotifyAll(); }
    [[nodiscard]] int getWaitCount() const { return impl().doGetWaitCount(); }

protected:
    ~ConditionVariableBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif /* OSAL_ENABLE_CONDITION_VAR */

#endif  // DP_OSAL_INTERFACE_CONDITION_VARIABLE_H_
