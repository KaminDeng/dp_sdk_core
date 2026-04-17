/** @file   dp_dac.h
 *  @brief  DAC device CRTP interface for dp_hal.
 *
 *  Impl must provide:
 *    - Status doConfigure(uint8_t channel, uint8_t resolution_bits)
 *    - Status doWrite(uint8_t channel, uint16_t value) */

#ifndef DP_DAC_H_
#define DP_DAC_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  DAC device interface (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class DacBase {
public:
    /** @brief  Configure a DAC channel.
     *  @param  channel          Channel number.
     *  @param  resolution_bits  DAC resolution (e.g., 8, 10, 12).
     *  @return Status::kOk on success. */
    Status configure(uint8_t channel, uint8_t resolution_bits) { return impl().doConfigure(channel, resolution_bits); }

    /** @brief  Write a value to a DAC channel.
     *  @param  channel  Channel number.
     *  @param  value    Output value.
     *  @return Status::kOk on success. */
    Status write(uint8_t channel, uint16_t value) { return impl().doWrite(channel, value); }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::hal

#endif  // DP_DAC_H_
