/** @file   dp_adc.h
 *  @brief  ADC device CRTP interface for dp_hal.
 *
 *  Impl must provide:
 *    - Status doConfigure(uint8_t channel, uint8_t resolution_bits)
 *    - uint16_t doRead(uint8_t channel)
 *    - Status doStartContinuous(uint8_t channel, ConversionCallback cb,
 *                                void* ctx)
 *    - Status doStopContinuous(uint8_t channel) */

#ifndef DP_ADC_H_
#define DP_ADC_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  ADC device interface (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class AdcBase {
public:
    /** @brief  Configure an ADC channel.
     *  @param  channel          Channel number.
     *  @param  resolution_bits  ADC resolution (e.g., 8, 10, 12, 16).
     *  @return Status::kOk on success. */
    Status configure(uint8_t channel, uint8_t resolution_bits) { return impl().doConfigure(channel, resolution_bits); }

    /** @brief  Read a single conversion from an ADC channel.
     *  @param  channel  Channel number.
     *  @return Converted value. */
    uint16_t read(uint8_t channel) { return impl().doRead(channel); }

    /** @brief  Continuous conversion callback type.
     *  @param  channel  Channel that produced the sample.
     *  @param  value    Converted value.
     *  @param  ctx      User context pointer. */
    using ConversionCallback = void (*)(uint8_t channel, uint16_t value, void *ctx);

    /** @brief  Start continuous conversion on a channel.
     *  @param  channel  Channel number.
     *  @param  cb       Callback invoked per conversion.
     *  @param  ctx      User context pointer.
     *  @return Status::kOk on success, kNotSupported if not available. */
    Status startContinuous(uint8_t channel, ConversionCallback cb, void *ctx) {
        return impl().doStartContinuous(channel, cb, ctx);
    }

    /** @brief  Stop continuous conversion on a channel.
     *  @param  channel  Channel number.
     *  @return Status::kOk on success. */
    Status stopContinuous(uint8_t channel) { return impl().doStopContinuous(channel); }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::hal

#endif  // DP_ADC_H_
