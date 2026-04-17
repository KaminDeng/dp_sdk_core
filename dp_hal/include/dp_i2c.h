/** @file   dp_i2c.h
 *  @brief  I2C bus controller and I2C device CRTP interfaces for dp_hal.
 *
 *  I2cBusBase Impl must provide:
 *    - Status doConfigure(const I2cConfig& cfg)
 *    - Status doWrite(uint16_t addr, const uint8_t* buf, size_t len)
 *    - Status doRead(uint16_t addr, uint8_t* buf, size_t len)
 *    - Status doWriteRead(uint16_t addr, const uint8_t* tx, size_t tx_len,
 *                          uint8_t* rx, size_t rx_len) */

#ifndef DP_I2C_H_
#define DP_I2C_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_port.h"
#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  I2C bus controller interface (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class I2cBusBase {
public:
    /** @brief  Configure the I2C bus.
     *  @param  cfg  I2C configuration parameters.
     *  @return Status::kOk on success. */
    Status configure(const I2cConfig &cfg) { return impl().doConfigure(cfg); }

    /** @brief  Write data to an I2C slave.
     *  @param  addr  7-bit slave address.
     *  @param  buf   Data buffer to transmit.
     *  @param  len   Number of bytes.
     *  @return Status::kOk on success. */
    Status write(uint16_t addr, const uint8_t *buf, size_t len) { return impl().doWrite(addr, buf, len); }

    /** @brief  Read data from an I2C slave.
     *  @param  addr  7-bit slave address.
     *  @param  buf   Receive buffer.
     *  @param  len   Number of bytes.
     *  @return Status::kOk on success. */
    Status read(uint16_t addr, uint8_t *buf, size_t len) { return impl().doRead(addr, buf, len); }

    /** @brief  Combined write-then-read (restart condition).
     *  @param  addr    7-bit slave address.
     *  @param  tx      Transmit buffer.
     *  @param  tx_len  Bytes to transmit.
     *  @param  rx      Receive buffer.
     *  @param  rx_len  Bytes to receive.
     *  @return Status::kOk on success. */
    Status writeRead(uint16_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) {
        return impl().doWriteRead(addr, tx, tx_len, rx, rx_len);
    }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

/** @brief  I2C slave device = bus + address (platform-independent framework).
 *
 *  Provides register-level read/write helpers. Uses a fixed-size internal
 *  buffer (max 32 data bytes per writeReg call) instead of VLA.
 *
 *  @tparam I2cBus  I2C bus controller type (must satisfy I2cBusBase). */
template <typename I2cBus>
class I2cDevice {
public:
    /** @brief  Maximum data bytes per single writeReg call. */
    static constexpr size_t kMaxRegWriteLen = 32;

    /** @brief  Construct an I2C device.
     *  @param  bus   Reference to the I2C bus controller.
     *  @param  addr  7-bit slave address. */
    I2cDevice(I2cBus &bus, uint16_t addr) : bus_(bus), addr_(addr) {}

    /** @brief  Write to a device register.
     *  @param  reg   Register address byte.
     *  @param  data  Data to write.
     *  @param  len   Number of data bytes (max kMaxRegWriteLen).
     *  @return Status::kOk on success, kInvalidArg if len exceeds limit. */
    Status writeReg(uint8_t reg, const uint8_t *data, size_t len) {
        DP_HAL_ASSERT(len <= kMaxRegWriteLen);
        if (len > kMaxRegWriteLen) {
            return Status::kInvalidArg;
        }
        uint8_t tx[kMaxRegWriteLen + 1];
        tx[0] = reg;
        for (size_t i = 0; i < len; ++i) {
            tx[i + 1] = data[i];
        }
        return bus_.write(addr_, tx, 1 + len);
    }

    /** @brief  Read from a device register.
     *  @param  reg   Register address byte.
     *  @param  data  Buffer to receive data.
     *  @param  len   Number of bytes to read.
     *  @return Status::kOk on success. */
    Status readReg(uint8_t reg, uint8_t *data, size_t len) { return bus_.writeRead(addr_, &reg, 1, data, len); }

private:
    I2cBus &bus_;
    uint16_t addr_;
};

}  // namespace dp::hal

#endif  // DP_I2C_H_
