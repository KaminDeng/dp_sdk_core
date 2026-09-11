
#pragma once
#if !DP_OSAL_ENABLE_LEGACY_COMPAT
#error "此旧接口仅由dp_osal_legacy_compat目标显式装配"
#endif


#include <cstdint>

namespace dp::osal {

using StaticTimerCallback = void (*)(void*);

enum class StaticTimerStatus : uint8_t {
    kOk = 0,
    kInvalidArgument,
    kAlreadyInitialized,
    kNotInitialized,
    kNotArmed,
    kNotDue,
};

class StaticTimerStorage {
public:
    StaticTimerStorage() = default;
    StaticTimerStorage(const StaticTimerStorage&) = delete;
    StaticTimerStorage& operator=(const StaticTimerStorage&) = delete;

private:
    StaticTimerCallback callback = nullptr;
    void* context = nullptr;
    uint32_t deadline = 0U;
    bool in_use = false;
    bool armed = false;

    friend class StaticTimer;
};

class StaticTimer {
public:
    StaticTimer() = default;
    StaticTimer(const StaticTimer&) = delete;
    StaticTimer& operator=(const StaticTimer&) = delete;
    StaticTimer(StaticTimer&&) = delete;
    StaticTimer& operator=(StaticTimer&&) = delete;
    ~StaticTimer() { reset(); }

    StaticTimerStatus init(StaticTimerStorage& storage, StaticTimerCallback callback,
                           void* context) {
        if (callback == nullptr) {
            return StaticTimerStatus::kInvalidArgument;
        }
        if (valid()) {
            return StaticTimerStatus::kAlreadyInitialized;
        }
        if (storage.in_use) {
            return StaticTimerStatus::kAlreadyInitialized;
        }
        storage.callback = callback;
        storage.context = context;
        storage.deadline = 0U;
        storage.armed = false;
        storage.in_use = true;
        storage_ = &storage;
        return StaticTimerStatus::kOk;
    }

    StaticTimerStatus arm_at(uint32_t absolute_tick) {
        if (!valid()) {
            return StaticTimerStatus::kNotInitialized;
        }
        storage_->deadline = absolute_tick;
        storage_->armed = true;
        return StaticTimerStatus::kOk;
    }

    StaticTimerStatus cancel() {
        if (!valid()) {
            return StaticTimerStatus::kNotInitialized;
        }
        storage_->armed = false;
        return StaticTimerStatus::kOk;
    }

    StaticTimerStatus dispatch_due(uint32_t now_tick) {
        if (!valid()) {
            return StaticTimerStatus::kNotInitialized;
        }
        if (!storage_->armed) {
            return StaticTimerStatus::kNotArmed;
        }
        const uint32_t elapsed = now_tick - storage_->deadline;
        if (now_tick != storage_->deadline && elapsed >= 0x80000000U) {
            return StaticTimerStatus::kNotDue;
        }
        StaticTimerCallback callback = storage_->callback;
        void* context = storage_->context;
        storage_->armed = false;
        callback(context);
        return StaticTimerStatus::kOk;
    }

    bool valid() const { return storage_ != nullptr && storage_->in_use; }
    bool armed() const { return valid() && storage_->armed; }

    void reset() {
        if (storage_ != nullptr && storage_->in_use) {
            storage_->callback = nullptr;
            storage_->context = nullptr;
            storage_->deadline = 0U;
            storage_->armed = false;
            storage_->in_use = false;
        }
        storage_ = nullptr;
    }

private:
    StaticTimerStorage* storage_ = nullptr;
};

}  // namespace dp::osal
