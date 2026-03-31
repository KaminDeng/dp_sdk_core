/** @file   dp_spi_device.h
 *  @brief  SpiBusDevice adapter: Device + ISpi, delegates to a HAL impl. */

#ifndef DP_SPI_DEVICE_H_
#define DP_SPI_DEVICE_H_

#include "dp_device.h"
#include "dp_hal_virtual.h"

namespace dp::device {

/** @brief  SPI bus device adapter.
 *
 *  Inherits Device for lifecycle management and ISpi for the SPI
 *  virtual interface.  All SPI operations delegate to the contained
 *  HAL implementation.
 *
 *  @tparam HalImpl  A concrete SpiBusBase CRTP implementation. */
template <typename HalImpl>
class SpiBusDevice : public Device, public dp::hal::ISpi {
public:
    /** @brief  Construct an SPI bus device.
     *  @param  name  Device name.
     *  @param  hal   Reference to the HAL SPI bus implementation. */
    SpiBusDevice(const char *name, HalImpl &hal) : Device(name, DeviceType::kSpiBus), hal_(hal) {}

    dp::hal::Status configure(const dp::hal::SpiConfig &cfg) override { return hal_.configure(cfg); }

    dp::hal::Status transfer(const uint8_t *tx, uint8_t *rx, size_t len) override { return hal_.transfer(tx, rx, len); }

    dp::hal::Status write(const uint8_t *buf, size_t len) override { return hal_.write(buf, len); }

    dp::hal::Status read(uint8_t *buf, size_t len) override { return hal_.read(buf, len); }

    dp::hal::Status transferAsync(const uint8_t *tx, uint8_t *rx, size_t len, dp::hal::SpiTransferCompleteCallback cb,
                                  void *ctx) override {
        return hal_.transferAsync(tx, rx, len, cb, ctx);
    }

private:
    HalImpl &hal_;
};

}  // namespace dp::device

#endif  // DP_SPI_DEVICE_H_
