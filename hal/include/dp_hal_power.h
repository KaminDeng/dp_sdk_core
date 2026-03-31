/** @file   dp_hal_power.h
 *  @brief  Optional power management CRTP mixin for dp_hal devices.
 *
 *  Devices supporting power management inherit from this in addition to their
 *  primary CRTP base:
 *  @code
 *  class Stm32Uart : public UartBase<Stm32Uart>,
 *                    public PowerManageable<Stm32Uart> { ... };
 *  @endcode
 *
 *  Impl must provide:
 *    - Status doSetPowerState(PowerState state)
 *    - PowerState doGetPowerState() */

#ifndef DP_HAL_POWER_H_
#define DP_HAL_POWER_H_

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  Optional power management mixin (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class PowerManageable {
public:
    /** @brief  Set the device power state.
     *  @param  state  Desired power state.
     *  @return Status::kOk on success. */
    Status setPowerState(PowerState state) { return impl().doSetPowerState(state); }

    /** @brief  Query the current device power state.
     *  @return Current power state. */
    PowerState getPowerState() { return impl().doGetPowerState(); }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::hal

#endif  // DP_HAL_POWER_H_
