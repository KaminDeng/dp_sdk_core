
#ifndef DP_OSAL_INTERFACE_SEMAPHORE_H_
#define DP_OSAL_INTERFACE_SEMAPHORE_H_

#include <cstdint>

#include "dp_osal_measurement.h"

namespace dp::osal {


template <typename Impl>
class SemaphoreBase {
public:

    void wait() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            impl().doWait();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kWake, started, detail::measurementNow(), true);
            return;
        }
#endif
        impl().doWait();
    }

    void signal() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            impl().doSignal();
            Measurement::record_signal(false); // 记录前再次检查开关，允许在途关闭后丢弃。
            return;
        }
#endif
        impl().doSignal();
    }

    void signalFromIsr() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if constexpr (detail::measurementThreadIsrTag()) {
            if (Measurement::enabled()) {
                impl().doSignalFromIsr();
                Measurement::record_signal(true);
                return;
            }
        }
#endif
        impl().doSignalFromIsr();
    }

    bool tryWait() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryWait();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kWake, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTryWait();
    }

    bool tryWaitFor(uint32_t timestamp) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryWaitFor(timestamp);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kWake, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTryWaitFor(timestamp);
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
