/** @file   dp_device.h
 *  @brief  Device base class: name, type, open/close lifecycle with refcount. */

#ifndef DP_DEVICE_H_
#define DP_DEVICE_H_

#include <stdint.h>
#include <string.h>

#include "dp_device_types.h"
#include "dp_hal_types.h"

namespace dp::device {

/** @brief  Maximum length of a device name (including NUL terminator). */
static constexpr size_t kDeviceNameMax = 32;

/** @brief  Base class for all managed devices.
 *
 *  Provides name/type identification and open/close reference counting.
 *  Subclasses override doOpen()/doClose() for hardware-specific behaviour. */
class Device {
public:
    /** @brief  Construct a named device of the given type.
     *  @param  name  Null-terminated device name (truncated to kDeviceNameMax-1).
     *  @param  type  Device class identifier. */
    Device(const char *name, DeviceType type) : type_(type) {
        if (name != nullptr) {
            strncpy(name_, name, kDeviceNameMax - 1);
            name_[kDeviceNameMax - 1] = '\0';
        } else {
            name_[0] = '\0';
        }
    }

    virtual ~Device() = default;

    /* Non-copyable, non-movable. */
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(Device &&) = delete;

    /** @brief  Open the device.
     *  @param  flags  Implementation-defined open flags.
     *  @return kOk on success, error status otherwise.
     *
     *  First open invokes doOpen(); subsequent opens increment the reference
     *  count without calling doOpen() again. */
    dp::hal::Status open(uint32_t flags = 0) {
        if (ref_count_ == 0) {
            dp::hal::Status st = doOpen(flags);
            if (st != dp::hal::Status::kOk) {
                return st;
            }
        }
        ++ref_count_;
        return dp::hal::Status::kOk;
    }

    /** @brief  Close the device.
     *  @return kOk on success, kError if the device is not open.
     *
     *  Decrements the reference count; calls doClose() when it reaches zero. */
    dp::hal::Status close() {
        if (ref_count_ == 0) {
            return dp::hal::Status::kError;
        }
        --ref_count_;
        if (ref_count_ == 0) {
            return doClose();
        }
        return dp::hal::Status::kOk;
    }

    /** @brief  Get the device name. */
    const char *name() const { return name_; }

    /** @brief  Get the device type. */
    DeviceType type() const { return type_; }

    /** @brief  Check whether the device is currently open. */
    bool isOpen() const { return ref_count_ > 0; }

    /** @brief  Get the current reference count. */
    uint32_t refCount() const { return ref_count_; }

    /** @brief  Get the primary dp_hal virtual interface pointer.
     *
     *  Subclasses override to return their dp::hal::IXxx* (e.g., IUart*, IGpioPin*).
     *  Used by dp_shell commands to access device-specific operations without RTTI.
     *  @return Pointer to the virtual interface, or nullptr if not applicable. */
    virtual void *interface() { return nullptr; }

protected:
    /** @brief  Hardware-specific open logic (called on first open).
     *  @param  flags  Implementation-defined open flags.
     *  @return kOk by default. */
    virtual dp::hal::Status doOpen(uint32_t /*flags*/) { return dp::hal::Status::kOk; }

    /** @brief  Hardware-specific close logic (called when refcount reaches 0).
     *  @return kOk by default. */
    virtual dp::hal::Status doClose() { return dp::hal::Status::kOk; }

private:
    char name_[kDeviceNameMax] = {};
    DeviceType type_;
    uint32_t ref_count_ = 0;
};

}  // namespace dp::device

#endif  // DP_DEVICE_H_
