/** @file   dp_device_manager.h
 *  @brief  Singleton device registry with thread-safe name-based lookup. */

#ifndef DP_DEVICE_MANAGER_H_
#define DP_DEVICE_MANAGER_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "dp_device.h"
#include "dp_hal_types.h"
#include "dp_osal_lockguard.h"
#include "dp_osal_mutex.h"

namespace dp::device {

/** @brief  Callback type for DeviceManager::forEach().
 *  @param  dev  Pointer to the iterated device.
 *  @param  ctx  User context pointer. */
using DeviceForEachCallback = void (*)(Device *dev, void *ctx);

/** @brief  Thread-safe singleton device registry.
 *
 *  Holds a fixed-size array of Device pointers (no heap allocation).
 *  All public methods are protected by an OSAL mutex. */
class DeviceManager {
public:
    /** @brief  Maximum number of simultaneously registered devices. */
    static constexpr size_t kMaxDevices = 32;

    /** @brief  Get the singleton instance. */
    static DeviceManager &instance() {
        static DeviceManager mgr;
        return mgr;
    }

    /** @brief  Register a device.
     *  @param  dev  Non-null pointer to a Device.
     *  @return kOk on success, kInvalidArg if null, kError if duplicate name
     *          or registry full. */
    dp::hal::Status registerDevice(Device *dev) {
        if (dev == nullptr) {
            return dp::hal::Status::kInvalidArg;
        }
        dp::osal::LockGuard lock(mutex_);
        /* Check duplicate name. */
        for (size_t i = 0; i < count_; ++i) {
            if (strcmp(devices_[i]->name(), dev->name()) == 0) {
                return dp::hal::Status::kError;
            }
        }
        /* Check capacity. */
        if (count_ >= kMaxDevices) {
            return dp::hal::Status::kError;
        }
        devices_[count_] = dev;
        ++count_;
        return dp::hal::Status::kOk;
    }

    /** @brief  Unregister a device by name.
     *  @param  name  Device name to remove.
     *  @return kOk on success, kBusy if the device is still open,
     *          kError if not found. */
    dp::hal::Status unregisterDevice(const char *name) {
        dp::osal::LockGuard lock(mutex_);
        for (size_t i = 0; i < count_; ++i) {
            if (strcmp(devices_[i]->name(), name) == 0) {
                if (devices_[i]->isOpen()) {
                    return dp::hal::Status::kBusy;
                }
                /* Shift remaining entries. */
                for (size_t j = i; j + 1 < count_; ++j) {
                    devices_[j] = devices_[j + 1];
                }
                devices_[count_ - 1] = nullptr;
                --count_;
                return dp::hal::Status::kOk;
            }
        }
        return dp::hal::Status::kError;
    }

    /** @brief  Find a device by name.
     *  @param  name  Device name to search for.
     *  @return Pointer to the device, or nullptr if not found. */
    Device *find(const char *name) {
        dp::osal::LockGuard lock(mutex_);
        for (size_t i = 0; i < count_; ++i) {
            if (strcmp(devices_[i]->name(), name) == 0) {
                return devices_[i];
            }
        }
        return nullptr;
    }

    /** @brief  Find a device by name and verify its type.
     *  @param  name           Device name to search for.
     *  @param  expected_type  Expected DeviceType.
     *  @return Pointer to the device, or nullptr if not found or type mismatch. */
    Device *findByType(const char *name, DeviceType expected_type) {
        Device *dev = find(name);
        if (dev != nullptr && dev->type() != expected_type) {
            return nullptr;
        }
        return dev;
    }

    /** @brief  Iterate all registered devices.
     *  @param  cb   Callback invoked for each device.
     *  @param  ctx  User context pointer forwarded to the callback. */
    void forEach(DeviceForEachCallback cb, void *ctx) {
        dp::osal::LockGuard lock(mutex_);
        for (size_t i = 0; i < count_; ++i) {
            cb(devices_[i], ctx);
        }
    }

    /** @brief  Get the number of registered devices. */
    size_t count() {
        dp::osal::LockGuard lock(mutex_);
        return count_;
    }

    /** @brief  Remove all registered devices (for testing). */
    void clear() {
        dp::osal::LockGuard lock(mutex_);
        for (size_t i = 0; i < count_; ++i) {
            devices_[i] = nullptr;
        }
        count_ = 0;
    }

private:
    DeviceManager() = default;
    ~DeviceManager() = default;
    DeviceManager(const DeviceManager &) = delete;
    DeviceManager &operator=(const DeviceManager &) = delete;

    dp::osal::Mutex mutex_;
    Device *devices_[kMaxDevices] = {};
    size_t count_ = 0;
};

}  // namespace dp::device

#endif  // DP_DEVICE_MANAGER_H_
