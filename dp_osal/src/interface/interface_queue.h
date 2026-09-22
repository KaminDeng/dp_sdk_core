
#ifndef DP_OSAL_INTERFACE_QUEUE_H_
#define DP_OSAL_INTERFACE_QUEUE_H_

#include <cstddef>
#include <cstdint>

#include "dp_osal_measurement.h"

namespace dp::osal {


template <typename Impl, typename T>
class QueueBase {
public:

    void send(const T &message) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            impl().doSend(message);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), true);
            return;
        }
#endif
        impl().doSend(message);
    }


    bool trySend(const T &message) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTrySend(message);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTrySend(message);
    }

    T receive() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            T message = impl().doReceive();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), true);
            return message;
        }
#endif
        return impl().doReceive();
    }

    bool tryReceive(T &message) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryReceive(message);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTryReceive(message);
    }

    bool receiveFor(T &message, uint32_t timeout) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doReceiveFor(message, timeout);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kQueue, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doReceiveFor(message, timeout);
    }

    [[nodiscard]] size_t size() const { return impl().doSize(); }

    void clear() { impl().doClear(); }

protected:

    ~QueueBase() = default;

private:

    Impl &impl() { return *static_cast<Impl *>(this); }

    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_QUEUE_H_
