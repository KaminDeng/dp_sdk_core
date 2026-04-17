/** @file   dp_i2c_device.h
 *  @brief  I2cBusDevice adapter: Device + II2c, delegates to a HAL impl. */

#ifndef DP_I2C_DEVICE_H_
#define DP_I2C_DEVICE_H_

#include "dp_device.h"
#include "dp_hal_virtual.h"

namespace dp::device {

/** @brief  I2C bus device adapter.
 *
 *  Inherits Device for lifecycle management and II2c for the I2C
 *  virtual interface.  All I2C operations delegate to the contained
 *  HAL implementation.
 *
 *  @tparam HalImpl  A concrete I2cBusBase CRTP implementation. */
template <typename HalImpl>
class I2cBusDevice : public Device, public dp::hal::II2c {
public:
    /** @brief  Construct an I2C bus device.
     *  @param  name  Device name.
     *  @param  hal   Reference to the HAL I2C bus implementation. */
    I2cBusDevice(const char *name, HalImpl &hal) : Device(name, DeviceType::kI2cBus), hal_(hal) {}

    dp::hal::Status configure(const dp::hal::I2cConfig &cfg) override { return hal_.configure(cfg); }

    dp::hal::Status write(uint16_t addr, const uint8_t *buf, size_t len) override { return hal_.write(addr, buf, len); }

    dp::hal::Status read(uint16_t addr, uint8_t *buf, size_t len) override { return hal_.read(addr, buf, len); }

    dp::hal::Status writeRead(uint16_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) override {
        return hal_.writeRead(addr, tx, tx_len, rx, rx_len);
    }

private:
    HalImpl &hal_;
};

}  // namespace dp::device

#endif  // DP_I2C_DEVICE_H_
