/** @file   dp_gpio.h
 *  @brief  GPIO per-pin CRTP interface for dp_hal.
 *
 *  Impl must provide:
 *    - Status doSetMode(PinMode mode)
 *    - Status doWrite(PinState state)
 *    - PinState doRead()
 *    - Status doToggle()
 *    - Status doEnableIrq(GpioIrqTrigger trigger, IrqCallback cb, void* ctx)
 *    - Status doDisableIrq() */

#ifndef DP_GPIO_H_
#define DP_GPIO_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  Per-pin GPIO interface (CRTP, Pigweed active/inactive style).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class GpioPinBase {
public:
    /** @brief  Set the pin electrical mode.
     *  @param  mode  Desired pin mode.
     *  @return Status::kOk on success. */
    Status setMode(PinMode mode) { return impl().doSetMode(mode); }

    /** @brief  Set the pin logical state.
     *  @param  state  Desired pin state.
     *  @return Status::kOk on success. */
    Status write(PinState state) { return impl().doWrite(state); }

    /** @brief  Read the current pin logical state.
     *  @return Current PinState. */
    PinState read() { return impl().doRead(); }

    /** @brief  Toggle the pin state.
     *  @return Status::kOk on success. */
    Status toggle() { return impl().doToggle(); }

    /** @brief  GPIO interrupt callback type.
     *  @param  ctx  User context pointer. */
    using IrqCallback = void (*)(void *ctx);

    /** @brief  Enable interrupt on the pin.
     *  @param  trigger  Edge trigger type.
     *  @param  cb       Callback function.
     *  @param  ctx      User context pointer.
     *  @return Status::kOk on success, kNotSupported if not available. */
    Status enableIrq(GpioIrqTrigger trigger, IrqCallback cb, void *ctx) { return impl().doEnableIrq(trigger, cb, ctx); }

    /** @brief  Disable interrupt on the pin.
     *  @return Status::kOk on success. */
    Status disableIrq() { return impl().doDisableIrq(); }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::hal

#endif  // DP_GPIO_H_
