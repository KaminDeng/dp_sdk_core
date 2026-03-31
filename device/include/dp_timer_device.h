/** @file   dp_timer_device.h
 *  @brief  TimerDevice adapter: Device + ITimer, delegates to a HAL impl. */

#ifndef DP_TIMER_DEVICE_H_
#define DP_TIMER_DEVICE_H_

#include "dp_device.h"
#include "dp_hal_virtual.h"

namespace dp::device {

/** @brief  Timer device adapter.
 *
 *  Inherits Device for lifecycle management and ITimer for the timer
 *  virtual interface.  All timer operations delegate to the contained
 *  HAL implementation.
 *
 *  @tparam HalImpl  A concrete TimerBase CRTP implementation. */
template <typename HalImpl>
class TimerDevice : public Device, public dp::hal::ITimer {
public:
    /** @brief  Construct a timer device.
     *  @param  name  Device name.
     *  @param  hal   Reference to the HAL timer implementation. */
    TimerDevice(const char *name, HalImpl &hal) : Device(name, DeviceType::kTimer), hal_(hal) {}

    dp::hal::Status start(uint32_t period_us) override { return hal_.start(period_us); }

    dp::hal::Status stop() override { return hal_.stop(); }

    dp::hal::Status setCallback(dp::hal::TimerCallback cb, void *ctx) override { return hal_.setCallback(cb, ctx); }

    uint32_t getCounterUs() override { return hal_.getCounterUs(); }

private:
    HalImpl &hal_;
};

}  // namespace dp::device

#endif  // DP_TIMER_DEVICE_H_
