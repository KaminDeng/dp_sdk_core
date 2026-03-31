/** @file   dp_hal_types.h
 *  @brief  Common types and enumerations for the dp_hal device abstraction. */

#ifndef DP_HAL_TYPES_H_
#define DP_HAL_TYPES_H_

#include <stddef.h>
#include <stdint.h>

namespace dp::hal {

/** @brief  HAL operation status codes. */
enum class Status : int8_t {
    kOk = 0,
    kError = -1,
    kTimeout = -2,
    kBusy = -3,
    kNotSupported = -4,
    kInvalidArg = -5,
};

/** @brief  GPIO pin electrical mode. */
enum class PinMode : uint8_t {
    kInput,
    kOutput,
    kInputPullUp,
    kInputPullDown,
    kOutputOpenDrain,
    kAnalog,
    kAlternate,
};

/** @brief  GPIO logical state (active/inactive, decoupled from electrical
 *          polarity). */
enum class PinState : uint8_t {
    kInactive = 0,
    kActive = 1,
};

/** @brief  GPIO interrupt trigger edge. */
enum class GpioIrqTrigger : uint8_t {
    kRising,
    kFalling,
    kBothEdges,
};

/** @brief  UART configuration. */
struct UartConfig {
    uint32_t baud_rate;  ///< Baud rate (e.g., 115200).
    uint8_t data_bits;   ///< Data bits: 7, 8, or 9.
    uint8_t stop_bits;   ///< Stop bits: 1 or 2.
    uint8_t parity;      ///< 0=none, 1=odd, 2=even.
};

/** @brief  SPI bus configuration. */
struct SpiConfig {
    uint32_t clock_hz;  ///< SPI clock frequency in Hz.
    uint8_t mode;       ///< CPOL/CPHA mode: 0-3.
    uint8_t bit_order;  ///< 0=MSB first, 1=LSB first.
};

/** @brief  I2C bus configuration. */
struct I2cConfig {
    uint32_t clock_hz;  ///< I2C clock: 100000, 400000, or 1000000.
};

/** @brief  Power state for devices supporting power management. */
enum class PowerState : uint8_t {
    kActive,     ///< Normal operation.
    kSleep,      ///< Low power, fast wake-up.
    kDeepSleep,  ///< Deep sleep, re-init on wake.
    kOff,        ///< Powered off.
};

}  // namespace dp::hal

#endif  // DP_HAL_TYPES_H_
