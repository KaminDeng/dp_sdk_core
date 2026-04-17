/** @file interface_timer.h
 *  @brief CRTP one-shot and periodic timer interface for OSAL. */
#ifndef OSAL_INTERFACE_TIMER_H_
#define OSAL_INTERFACE_TIMER_H_

#include <cstdint>
#include <functional>

#if OSAL_ENABLE_TIMER

namespace osal {

template <typename Impl>
class TimerBase {
public:
    void start(uint32_t interval, bool periodic, std::function<void()> callback) {
        impl().doStart(interval, periodic, callback);
    }
    void stop() { impl().doStop(); }
    [[nodiscard]] bool isRunning() const { return impl().doIsRunning(); }
    [[nodiscard]] uint32_t getRemainingTime() const { return impl().doGetRemainingTime(); }
    void reset() { impl().doReset(); }

protected:
    ~TimerBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace osal

#endif /* OSAL_ENABLE_TIMER */

#endif  // OSAL_INTERFACE_TIMER_H_
