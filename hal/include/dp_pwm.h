/** @file   dp_pwm.h
 *  @brief  PWM output CRTP interface for dp_hal.
 *
 *  Impl must provide:
 *    - Status doStart(uint32_t frequency_hz, float duty_percent)
 *    - Status doSetDuty(float duty_percent)
 *    - Status doSetFrequency(uint32_t frequency_hz)
 *    - Status doStop() */

#ifndef DP_PWM_H_
#define DP_PWM_H_

#include <stdint.h>

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  PWM output interface (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class PwmBase {
public:
    /** @brief  Start PWM output at the specified frequency and duty cycle.
     *  @param  frequency_hz  PWM frequency in Hz.
     *  @param  duty_percent  Duty cycle (0.0 - 100.0).
     *  @return Status::kOk on success. */
    Status start(uint32_t frequency_hz, float duty_percent) {
        return impl().doStart(frequency_hz, duty_percent);
    }

    /** @brief  Update the duty cycle (frequency unchanged).
     *  @param  duty_percent  Duty cycle (0.0 - 100.0).
     *  @return Status::kOk on success. */
    Status setDuty(float duty_percent) {
        return impl().doSetDuty(duty_percent);
    }

    /** @brief  Update the frequency (duty cycle ratio preserved).
     *  @param  frequency_hz  New PWM frequency in Hz.
     *  @return Status::kOk on success. */
    Status setFrequency(uint32_t frequency_hz) {
        return impl().doSetFrequency(frequency_hz);
    }

    /** @brief  Stop PWM output.
     *  @return Status::kOk on success. */
    Status stop() { return impl().doStop(); }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::hal

#endif  // DP_PWM_H_
