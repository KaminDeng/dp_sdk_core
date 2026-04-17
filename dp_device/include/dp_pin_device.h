/** @file   dp_pin_device.h
 *  @brief  PinDevice adapter: Device + IGpioPin, delegates to a HAL impl. */

#ifndef DP_PIN_DEVICE_H_
#define DP_PIN_DEVICE_H_

#include "dp_device.h"
#include "dp_hal_virtual.h"

namespace dp::device {

/** @brief  GPIO pin device adapter.
 *
 *  Inherits Device for lifecycle management and IGpioPin for the GPIO
 *  virtual interface.  All GPIO operations delegate to the contained
 *  HAL implementation.
 *
 *  @tparam HalImpl  A concrete GpioPinBase CRTP implementation. */
template <typename HalImpl>
class PinDevice : public Device, public dp::hal::IGpioPin {
public:
    /** @brief  Construct a pin device.
     *  @param  name  Device name.
     *  @param  hal   Reference to the HAL GPIO implementation. */
    PinDevice(const char *name, HalImpl &hal) : Device(name, DeviceType::kPin), hal_(hal) {}

    dp::hal::Status setMode(dp::hal::PinMode mode) override { return hal_.setMode(mode); }

    dp::hal::Status write(dp::hal::PinState state) override { return hal_.write(state); }

    dp::hal::PinState read() override { return hal_.read(); }

    dp::hal::Status toggle() override { return hal_.toggle(); }
    dp::hal::Status enableIrq(dp::hal::GpioIrqTrigger trigger, void (*cb)(void *ctx), void *ctx) override {
        return hal_.enableIrq(trigger, cb, ctx);
    }
    dp::hal::Status disableIrq() override { return hal_.disableIrq(); }

    void *interface() override { return static_cast<dp::hal::IGpioPin *>(this); }

private:
    HalImpl &hal_;
};

}  // namespace dp::device

#endif  // DP_PIN_DEVICE_H_
