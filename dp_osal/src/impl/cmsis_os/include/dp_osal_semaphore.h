/** @file dp_osal_semaphore.h
 * @brief CMSIS-RTOS2计数信号量。默认构造上限16、零初值；DeferredInit不取得资源。
 * waitForMs明确毫秒并按真实内核频率向上取整；既有tryWaitFor保留原生tick单位。
 * close/init只允许在全部使用者退出后执行。 */
#pragma once
#include "interface_semaphore.h"
#include "dp_osal_port.h"
#include "dp_osal_debug.h"
namespace dp::osal {
class Semaphore : public SemaphoreBase<Semaphore> {
    friend class SemaphoreBase<Semaphore>;

public:
    struct DeferredInit {};
    explicit Semaphore(uint32_t maximum = 16U) noexcept : maximum_(maximum) { (void)doOpen(0U, maximum); }
    explicit Semaphore(DeferredInit) noexcept {}
    ~Semaphore() { (void)doClose(); }
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

private:
    bool doOpen(uint32_t initial, uint32_t maximum) noexcept {
        if (semaphore_ != nullptr) return false;
        if (maximum == 0U) maximum = 16U;
        if (maximum == 0U || maximum > INT32_MAX || initial > maximum) return false;
        osSemaphoreAttr_t attr{};
        attr.name = "Semaphore";
        auto handle = osSemaphoreNew(maximum, initial, &attr);
        if (handle == nullptr) return false;
        semaphore_ = handle;
        maximum_ = maximum;
        return true;
    }
    bool doClose() noexcept {
        if (semaphore_ == nullptr) return true;
        const auto handle = semaphore_;
        semaphore_ = nullptr;
        return osSemaphoreDelete(handle) == osOK;
    }
    bool doRelease() noexcept { return semaphore_ != nullptr && osSemaphoreRelease(semaphore_) == osOK; }
    WaitResult acquire(uint32_t ticks) noexcept {
        if (semaphore_ == nullptr) return WaitResult::kError;
        const auto status = osSemaphoreAcquire(semaphore_, ticks);
        if (status == osOK) return WaitResult::kAcquired;
        if (status == osErrorTimeout || (ticks == 0U && status == osErrorResource)) return WaitResult::kTimeout;
        return WaitResult::kError;
    }
    WaitResult doWaitForMs(uint32_t milliseconds) noexcept {
        if (semaphore_ == nullptr) return WaitResult::kError;
        const uint32_t frequency = osKernelGetTickFreq();
        if (frequency == 0U) return WaitResult::kError;
        const uint64_t ticks = (static_cast<uint64_t>(milliseconds) * frequency + 999U) / 1000U;
        if (ticks >= osWaitForever) return WaitResult::kError;
        return acquire(static_cast<uint32_t>(ticks));
    }
    uint32_t doMaxCount() const noexcept { return maximum_; }
    void doWait() noexcept { (void)acquire(osWaitForever); }
    void doSignal() noexcept { (void)doRelease(); }
    void doSignalFromIsr() noexcept { (void)doRelease(); }
    bool doTryWait() noexcept { return acquire(0U) == WaitResult::kAcquired; }
    bool doTryWaitFor(uint32_t ticks) noexcept { return acquire(ticks) == WaitResult::kAcquired; }
    int doGetValue() const noexcept {
        return semaphore_ == nullptr ? 0 : static_cast<int>(osSemaphoreGetCount(semaphore_));
    }
    void doInit(int initial) noexcept {
        if (initial < 0 || static_cast<uint32_t>(initial) > maximum_) return;
        if (semaphore_ == nullptr) {
            (void)doOpen(static_cast<uint32_t>(initial), maximum_);
            return;
        }
        while (acquire(0U) == WaitResult::kAcquired) {
        }
        for (int i = 0; i < initial; ++i)
            if (!doRelease()) break;
    }
    uint32_t maximum_ = 16U;
    osSemaphoreId_t semaphore_ = nullptr;
};
}  // namespace dp::osal
