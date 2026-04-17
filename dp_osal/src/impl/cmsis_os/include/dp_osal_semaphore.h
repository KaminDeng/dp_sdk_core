//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_SEMAPHORE_H_
#define DP_OSAL_CMSIS_SEMAPHORE_H_

#include "interface_semaphore.h"
#include "osal.h"
#include "dp_osal_debug.h"

namespace dp::osal {

class Semaphore : public SemaphoreBase<Semaphore> {
    friend class SemaphoreBase<Semaphore>;

public:
    explicit Semaphore(uint32_t max_count = 16U) : semaphore_(nullptr) {
        osSemaphoreAttr_t semAttr = {};
        semAttr.name = "Semaphore";
        semaphore_ = osSemaphoreNew(max_count, 0U, &semAttr);
        if (semaphore_ == nullptr) {
            DP_OSAL_LOGE("Failed to create semaphore\n");
        } else {
            DP_OSAL_LOGD("Semaphore initialized\n");
        }
    }

    ~Semaphore() {
        if (semaphore_ != nullptr) {
            osSemaphoreDelete(semaphore_);
            DP_OSAL_LOGD("Semaphore destroyed\n");
        }
    }

private:
    void doWait() {
        if (osSemaphoreAcquire(semaphore_, osWaitForever) == osOK) {
            DP_OSAL_LOGD("Semaphore wait succeeded\n");
        } else {
            DP_OSAL_LOGE("Semaphore wait failed\n");
        }
    }

    void doSignal() {
        if (osSemaphoreRelease(semaphore_) == osOK) {
            DP_OSAL_LOGD("Semaphore signal succeeded\n");
        } else {
            DP_OSAL_LOGE("Semaphore signal failed\n");
        }
    }

    bool doTryWait() { return doTryWaitFor(0); }

    bool doTryWaitFor(const uint32_t timestamp) {
        if (osSemaphoreAcquire(semaphore_, timestamp) == osOK) {
            DP_OSAL_LOGD("Semaphore tryWait succeeded\n");
            return true;
        }
        DP_OSAL_LOGD("Semaphore tryWait failed\n");
        return false;
    }

    [[nodiscard]] int doGetValue() const {
        int count = (int)osSemaphoreGetCount(semaphore_);
        DP_OSAL_LOGD("Semaphore value: %d\n", count);
        return count;
    }

    void doInit(int initialValue) {
        if (semaphore_ != nullptr) {
            /* Drain the existing semaphore to zero: this is safe when no
             * threads are blocked in wait() — the precondition callers must
             * guarantee.  Re-creating the semaphore handle (old approach) left
             * any blocked threads with a dangling handle, causing undefined
             * behaviour (HardFault on MCU). */
            while (osSemaphoreGetCount(semaphore_) > 0) {
                osSemaphoreAcquire(semaphore_, 0);
            }
            /* Now raise the count to the requested initial value. */
            for (int i = 0; i < initialValue; ++i) {
                if (osSemaphoreRelease(semaphore_) != osOK) {
                    DP_OSAL_LOGE("Semaphore init: release %d failed (exceeds max count?)\n", i);
                    break;
                }
            }
            DP_OSAL_LOGD("Semaphore re-initialized to value %d\n", initialValue);
        } else {
            /* First-time init (constructor set semaphore_ to nullptr — shouldn't
             * happen in normal flow, but guard defensively). */
            osSemaphoreAttr_t semAttr = {};
            semAttr.name = "Semaphore";
            semaphore_ = osSemaphoreNew(16, (uint32_t)initialValue, &semAttr);
            if (semaphore_ == nullptr) {
                DP_OSAL_LOGE("Failed to initialize semaphore with value %d\n", initialValue);
            } else {
                DP_OSAL_LOGD("Semaphore initialized with value %d\n", initialValue);
            }
        }
    }
    osSemaphoreId_t semaphore_;
};

} // namespace dp::osal

#endif  // DP_OSAL_CMSIS_SEMAPHORE_H_
