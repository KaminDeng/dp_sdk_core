/**
 * @file dp_osal_semaphore.h
 * @brief 将 OSAL 计数信号量转发到 CMSIS-RTOS2 对象。
 * @details 等待与发送由 SemaphoreBase 包装计时，本文件负责底层资源与计数。
 * 相对超时参数直接作为内核 tick；构造失败只记录日志，对象仍存在，调用者不能据此假定句柄有效。
 * 销毁、重设计数须由调用者与所有使用者协调，不能在等待线程仍使用资源时销毁。
 */
//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_SEMAPHORE_H_
#define DP_OSAL_CMSIS_SEMAPHORE_H_

#include "interface_semaphore.h"
#include "dp_osal_port.h"
#include "dp_osal_debug.h"

namespace dp::osal {

/**
 * @brief 持有 CMSIS osSemaphoreId_t 的计数信号量，默认上限 16。
 */
class Semaphore : public SemaphoreBase<Semaphore> {
    friend class SemaphoreBase<Semaphore>;

public:
    /**
     * @brief 创建零初值的 CMSIS 信号量。
     * @param max_count 最大令牌数，默认 16，须符合内核约束。
     * @note 失败时句柄为空，仅写日志，不抛异常或返回构造状态。
     */
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

    /**
     * @brief 若句柄非空则请求删除 CMSIS 信号量，返回状态被忽略。
     * @pre 所有使用者已停止，不能让等待者继续持有被删除的对象。
     */
    ~Semaphore() {
        if (semaphore_ != nullptr) {
            osSemaphoreDelete(semaphore_);
            DP_OSAL_LOGD("Semaphore destroyed\n");
        }
    }

private:
    /**
     * @brief 以 osWaitForever 请求消费一枚令牌，内核失败只写错误日志。
     * @note void 返回不能供上层判断是否真的取得令牌。
     */
    void doWait() {
        if (osSemaphoreAcquire(semaphore_, osWaitForever) == osOK) {
            DP_OSAL_LOGD("Semaphore wait succeeded\n");
        } else {
            DP_OSAL_LOGE("Semaphore wait failed\n");
        }
    }

    /**
     * @brief 请求释放一枚令牌，满计数等错误只记录日志。
     * @note 释放成功不表示等待线程已经获得调度。
     */
    void doSignal() {
        if (osSemaphoreRelease(semaphore_) == osOK) {
            DP_OSAL_LOGD("Semaphore signal succeeded\n");
        } else {
            DP_OSAL_LOGE("Semaphore signal failed\n");
        }
    }

    /**
     * @brief 调用 osSemaphoreRelease 并丢弃返回状态。
     * @note 中断优先级和可调用范围由所选 CMSIS 内核端口保证；满计数失败不会传给调用者。
     */
    void doSignalFromIsr() { (void)osSemaphoreRelease(semaphore_); }

    /**
     * @brief 使用零 tick 超时尝试获取令牌。
     * @return osSemaphoreAcquire 返回 osOK 时为 true。
     */
    bool doTryWait() { return doTryWaitFor(0); }

    /**
     * @brief 以指定 CMSIS tick 超时请求消费令牌。
     * @param timestamp 相对等待量，单位为原生内核 tick；不是毫秒，也不是绝对时间戳。
     * @return 成功消费为 true，超时或内核错误为 false。
     */
    bool doTryWaitFor(const uint32_t timestamp) {
        if (osSemaphoreAcquire(semaphore_, timestamp) == osOK) {
            DP_OSAL_LOGD("Semaphore tryWait succeeded\n");
            return true;
        }
        DP_OSAL_LOGD("Semaphore tryWait failed\n");
        return false;
    }

    /**
     * @brief 获取当前令牌计数的瞬时观测。
     * @return osSemaphoreGetCount 的结果转为 int；不额外返回内核错误状态。
     * @note 读取不预留令牌，并发等待可立即消费该计数。
     */
    [[nodiscard]] int doGetValue() const {
        int count = (int)osSemaphoreGetCount(semaphore_);
        DP_OSAL_LOGD("Semaphore value: %d\n", count);
        return count;
    }

    /**
     * @brief 复用现有句柄排空到零，再释放到目标初值；空句柄时尝试以最大 16 新建。
     * @param initialValue 目标非负计数，调用者须满足后端范围。
     * @pre 准备阶段调用，保证没有并发等待、发送或其他重设操作。
     * @note 超过最大值可提前停止释放且仅写日志；不会重建有效句柄来修复等待者。
     */
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
    osSemaphoreId_t semaphore_;  ///< 自持内核句柄，创建失败时为空；删除只在析构进行。
};

} // namespace dp::osal

#endif  // DP_OSAL_CMSIS_SEMAPHORE_H_
