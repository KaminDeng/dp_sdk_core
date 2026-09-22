/** @file interface_semaphore.h
 *  @brief CRTP semaphore interface for OSAL. */
#ifndef DP_OSAL_INTERFACE_SEMAPHORE_H_
#define DP_OSAL_INTERFACE_SEMAPHORE_H_

#include <cstdint>

#include "dp_osal_measurement.h"

namespace dp::osal {

/** @brief CRTP base for semaphore implementations. */
template <typename Impl>
class SemaphoreBase {
public:
    void wait() {
        const uint32_t started = Measurement::now_us();
        impl().doWait();
        Measurement::record_wait(SynchronizationKind::kWake, started, Measurement::now_us(), true);
    }
    void signal() {
        impl().doSignal();
        Measurement::record_signal(false);
    }
    void signalFromIsr() {
        impl().doSignalFromIsr();
        Measurement::record_signal(true);
    }
    bool tryWait() {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doTryWait();
        Measurement::record_wait(SynchronizationKind::kWake, started, Measurement::now_us(), result);
        return result;
    }
    bool tryWaitFor(uint32_t timestamp) {
        const uint32_t started = Measurement::now_us();
        const bool result = impl().doTryWaitFor(timestamp);
        Measurement::record_wait(SynchronizationKind::kWake, started, Measurement::now_us(), result);
        return result;
    }
    [[nodiscard]] int getValue() const { return impl().doGetValue(); }
    void init(int initialValue) { impl().doInit(initialValue); }

protected:
    ~SemaphoreBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_SEMAPHORE_H_
