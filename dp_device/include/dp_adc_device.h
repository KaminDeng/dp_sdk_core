/** @file   dp_adc_device.h
 *  @brief  AdcDevice adapter: Device + IAdc, delegates to a HAL impl. */

#ifndef DP_ADC_DEVICE_H_
#define DP_ADC_DEVICE_H_

#include "dp_device.h"
#include "dp_hal_virtual.h"

namespace dp::device {

/** @brief  ADC device adapter.
 *
 *  Inherits Device for lifecycle management and IAdc for the ADC
 *  virtual interface.  All ADC operations delegate to the contained
 *  HAL implementation.
 *
 *  @tparam HalImpl  A concrete AdcBase CRTP implementation. */
template <typename HalImpl>
class AdcDevice : public Device, public dp::hal::IAdc {
public:
    /** @brief  Construct an ADC device.
     *  @param  name  Device name.
     *  @param  hal   Reference to the HAL ADC implementation. */
    AdcDevice(const char *name, HalImpl &hal) : Device(name, DeviceType::kAdc), hal_(hal) {}

    dp::hal::Status configure(uint8_t channel, uint8_t resolution_bits) override {
        return hal_.configure(channel, resolution_bits);
    }

    uint16_t read(uint8_t channel) override { return hal_.read(channel); }

    dp::hal::Status startContinuous(uint8_t channel, dp::hal::AdcConversionCallback cb, void *ctx) override {
        return hal_.startContinuous(channel, cb, ctx);
    }

    dp::hal::Status stopContinuous(uint8_t channel) override { return hal_.stopContinuous(channel); }

private:
    HalImpl &hal_;
};

}  // namespace dp::device

#endif  // DP_ADC_DEVICE_H_
