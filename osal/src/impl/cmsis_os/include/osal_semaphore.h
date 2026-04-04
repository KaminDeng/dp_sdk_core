//
// Created by kamin.deng on 2024/8/22.
//
#ifndef __OSAL_SEMAPHORE_H__
#define __OSAL_SEMAPHORE_H__

#include "interface_semaphore.h"
#include "osal.h"
#include "osal_debug.h"

namespace osal {

class OSALSemaphore : public SemaphoreBase<OSALSemaphore> {
    friend class SemaphoreBase<OSALSemaphore>;

public:
    OSALSemaphore() : semaphore_(nullptr) {
        osSemaphoreAttr_t semAttr = {};
        semAttr.name = "OSALSemaphore";
        semaphore_ = osSemaphoreNew(16, 0, &semAttr);  // max_count=16 supports counting semaphores; initial count=0
        if (semaphore_ == nullptr) {
            OSAL_LOGE("Failed to create semaphore\n");
            // 处理创建失败的情况
        } else {
            OSAL_LOGD("Semaphore initialized\n");
        }
    }

    ~OSALSemaphore() {
        if (semaphore_ != nullptr) {
            osSemaphoreDelete(semaphore_);
            OSAL_LOGD("Semaphore destroyed\n");
        }
    }

private:
    void doWait() {
        if (osSemaphoreAcquire(semaphore_, osWaitForever) == osOK) {
            OSAL_LOGD("Semaphore wait succeeded\n");
        } else {
            OSAL_LOGE("Semaphore wait failed\n");
        }
    }

    void doSignal() {
        if (osSemaphoreRelease(semaphore_) == osOK) {
            OSAL_LOGD("Semaphore signal succeeded\n");
        } else {
            OSAL_LOGE("Semaphore signal failed\n");
        }
    }

    bool doTryWait() { return doTryWaitFor(0); }

    bool doTryWaitFor(const uint32_t timestamp) {
        if (osSemaphoreAcquire(semaphore_, timestamp) == osOK) {
            OSAL_LOGD("Semaphore tryWait succeeded\n");
            return true;
        }
        OSAL_LOGD("Semaphore tryWait failed\n");
        return false;
    }

    [[nodiscard]] int doGetValue() const {
        int count = (int)osSemaphoreGetCount(semaphore_);
        OSAL_LOGD("Semaphore value: %d\n", count);
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
                    OSAL_LOGE("Semaphore init: release %d failed (exceeds max count?)\n", i);
                    break;
                }
            }
            OSAL_LOGD("Semaphore re-initialized to value %d\n", initialValue);
        } else {
            /* First-time init (constructor set semaphore_ to nullptr — shouldn't
             * happen in normal flow, but guard defensively). */
            osSemaphoreAttr_t semAttr = {};
            semAttr.name = "OSALSemaphore";
            semaphore_ = osSemaphoreNew(16, (uint32_t)initialValue, &semAttr);
            if (semaphore_ == nullptr) {
                OSAL_LOGE("Failed to initialize semaphore with value %d\n", initialValue);
            } else {
                OSAL_LOGD("Semaphore initialized with value %d\n", initialValue);
            }
        }
    }
    osSemaphoreId_t semaphore_;
};

}  // namespace osal

#endif  // __OSAL_SEMAPHORE_H__
