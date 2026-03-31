/** @file   dp_serial_device.h
 *  @brief  SerialDevice adapter: Device + IUart, delegates to a HAL impl. */

#ifndef DP_SERIAL_DEVICE_H_
#define DP_SERIAL_DEVICE_H_

#include "dp_device.h"
#include "dp_hal_virtual.h"

namespace dp::device {

/** @brief  Serial device adapter.
 *
 *  Inherits Device for lifecycle management and IUart for the UART
 *  virtual interface.  All UART operations delegate to the contained
 *  HAL implementation.
 *
 *  @tparam HalImpl  A concrete UartBase CRTP implementation. */
template <typename HalImpl>
class SerialDevice : public Device, public dp::hal::IUart {
public:
    /** @brief  Construct a serial device.
     *  @param  name  Device name.
     *  @param  hal   Reference to the HAL UART implementation. */
    SerialDevice(const char *name, HalImpl &hal) : Device(name, DeviceType::kSerial), hal_(hal) {}

    dp::hal::Status configure(const dp::hal::UartConfig &cfg) override { return hal_.configure(cfg); }

    dp::hal::Status write(const uint8_t *buf, size_t len) override { return hal_.write(buf, len); }

    dp::hal::Status read(uint8_t *buf, size_t max_len, size_t *actual) override {
        return hal_.read(buf, max_len, actual);
    }

    dp::hal::Status flush() override { return hal_.flush(); }

    void *interface() override { return static_cast<dp::hal::IUart *>(this); }

private:
    HalImpl &hal_;
};

}  // namespace dp::device

#endif  // DP_SERIAL_DEVICE_H_
