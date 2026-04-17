/** @file   dp_dac_device.h
 *  @brief  DacDevice adapter: Device + IDac, delegates to a HAL impl. */

#ifndef DP_DAC_DEVICE_H_
#define DP_DAC_DEVICE_H_

#include "dp_device.h"
#include "dp_hal_virtual.h"

namespace dp::device {

/** @brief  DAC device adapter.
 *
 *  Inherits Device for lifecycle management and IDac for the DAC
 *  virtual interface.  All DAC operations delegate to the contained
 *  HAL implementation.
 *
 *  @tparam HalImpl  A concrete DacBase CRTP implementation. */
template <typename HalImpl>
class DacDevice : public Device, public dp::hal::IDac {
public:
    /** @brief  Construct a DAC device.
     *  @param  name  Device name.
     *  @param  hal   Reference to the HAL DAC implementation. */
    DacDevice(const char *name, HalImpl &hal) : Device(name, DeviceType::kDac), hal_(hal) {}

    dp::hal::Status configure(uint8_t channel, uint8_t resolution_bits) override {
        return hal_.configure(channel, resolution_bits);
    }

    dp::hal::Status write(uint8_t channel, uint16_t value) override { return hal_.write(channel, value); }

private:
    HalImpl &hal_;
};

}  // namespace dp::device

#endif  // DP_DAC_DEVICE_H_
