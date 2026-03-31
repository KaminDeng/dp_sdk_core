/** @file   dp_timer.h
 *  @brief  Hardware timer CRTP interface for dp_hal.
 *
 *  Impl must provide:
 *    - Status doStart(uint32_t period_us)
 *    - Status doStop()
 *    - Status doSetCallback(Callback cb, void* ctx)
 *    - uint32_t doGetCounterUs() */

#ifndef DP_TIMER_H_
#define DP_TIMER_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  Hardware timer interface (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class TimerBase {
public:
    /** @brief  Timer callback type.
     *  @param  ctx  User context pointer. */
    using Callback = void (*)(void *ctx);

    /** @brief  Start the timer with a periodic interval.
     *  @param  period_us  Period in microseconds.
     *  @return Status::kOk on success. */
    Status start(uint32_t period_us) { return impl().doStart(period_us); }

    /** @brief  Stop the timer.
     *  @return Status::kOk on success. */
    Status stop() { return impl().doStop(); }

    /** @brief  Register a timer callback.
     *  @param  cb   Callback function.
     *  @param  ctx  User context pointer.
     *  @return Status::kOk on success. */
    Status setCallback(Callback cb, void *ctx) { return impl().doSetCallback(cb, ctx); }

    /** @brief  Read the current counter value in microseconds.
     *  @return Counter value in microseconds. */
    uint32_t getCounterUs() { return impl().doGetCounterUs(); }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::hal

#endif  // DP_TIMER_H_
