/** @file   dp_device_types.h
 *  @brief  Device type enumeration and helpers. */
#ifndef DP_DEVICE_TYPES_H_
#define DP_DEVICE_TYPES_H_

#include <stdint.h>

namespace dp::device {

/** @brief  Device class identifier (no RTTI needed). */
enum class DeviceType : uint8_t {
    kSerial,
    kPin,
    kSpiBus,
    kI2cBus,
    kAdc,
    kDac,
    kTimer,
};

/** @brief  Return human-readable name for a DeviceType. */
inline const char *deviceTypeName(DeviceType t) {
    switch (t) {
        case DeviceType::kSerial:
            return "Serial";
        case DeviceType::kPin:
            return "Pin";
        case DeviceType::kSpiBus:
            return "SpiBus";
        case DeviceType::kI2cBus:
            return "I2cBus";
        case DeviceType::kAdc:
            return "ADC";
        case DeviceType::kDac:
            return "DAC";
        case DeviceType::kTimer:
            return "Timer";
    }
    return "Unknown";
}

}  // namespace dp::device

#endif  // DP_DEVICE_TYPES_H_
