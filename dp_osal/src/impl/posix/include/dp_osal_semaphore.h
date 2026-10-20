/**
 * @file dp_osal_semaphore.h
 * @brief 以互斥锁、条件变量及整数计数实现的 POSIX 计数信号量。
 * @details 等待与发送由 SemaphoreBase 包装计时，本文件负责底层资源与计数。
 * 相对超时按毫秒解释；signalFromIsr 仍使用 std::mutex，不可在硬件 ISR 或异步信号处理器调用。
 * 销毁、重设计数须由调用者与所有使用者协调，不能在等待线程仍使用资源时销毁。
 */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_POSIX_SEMAPHORE_H_
#define DP_OSAL_POSIX_SEMAPHORE_H_

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "interface_semaphore.h"
#include "dp_osal_debug.h"

namespace dp::osal {

/**
 * @brief 主机计数信号量，令牌数保存在受 mutex 保护的 int 中。
 */
class Semaphore : public SemaphoreBase<Semaphore> {
    friend class SemaphoreBase<Semaphore>;

public:
    /**
     * @brief 以零令牌构造主机对象，不预先唤醒任何线程。
     */
    Semaphore() : count_(0) { DP_OSAL_LOGD("Semaphore initialized\n"); }

    /**
     * @brief 销毁主机成员并输出调试日志，调用者须保证没有等待者/并发使用者。
     */
    ~Semaphore() { DP_OSAL_LOGD("Semaphore destroyed\n"); }

private:
    /**
     * @brief 等待谓词 count_>0 成立后消费一枚令牌，循环谓词屏蔽虚假唤醒。
     */
    void doWait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return count_ > 0; });
        --count_;
        DP_OSAL_LOGD("Semaphore wait succeeded\n");
    }

    /**
     * @brief 持锁增加计数并通知一个条件变量等待者。
     * @note 没有显式最大值检查，调用者必须避免 int 计数溢出。
     */
    void doSignal() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        cond_.notify_one();
        DP_OSAL_LOGD("Semaphore signal succeeded\n");
    }

    /**
     * @brief 主机兼容入口：仍持 std::mutex 增计数并通知，不是异步信号安全函数。
     */
    void doSignalFromIsr() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        cond_.notify_one();
    }

    /**
     * @brief 持锁查看计数，非零时消费一枚，不等待未来令牌。
     * @return 取得令牌为 true，否则 false；获取内部 mutex 本身仍可能等待。
     */
    bool doTryWait() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ > 0) {
            --count_;
            DP_OSAL_LOGD("Semaphore tryWait succeeded\n");
            return true;
        }
        DP_OSAL_LOGD("Semaphore tryWait failed\n");
        return false;
    }

    /**
     * @brief 以 steady_clock 构造相对毫秒期限，条件成立则消费令牌。
     * @param timestamp 相对等待量，单位为毫秒；不是墙钟或绝对时间戳。
     * @return 成功消费为 true，期限内无令牌为 false。
     */
    bool doTryWaitFor(uint32_t timestamp) {
        std::unique_lock<std::mutex> lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        if (cond_.wait_until(lock, now + std::chrono::milliseconds(timestamp), [this] { return count_ > 0; })) {
            --count_;
            DP_OSAL_LOGD("Semaphore tryWaitFor succeeded\n");
            return true;
        }
        DP_OSAL_LOGD("Semaphore tryWaitFor failed\n");
        return false;
    }

    /**
     * @brief 获取当前令牌计数的瞬时观测。
     * @return 持锁复制的 int 计数。
     * @note 读取不预留令牌，并发等待可立即消费该计数。
     */
    int doGetValue() const {
        std::lock_guard<std::mutex> lock(mutex_);
        DP_OSAL_LOGD("Semaphore value: %d\n", count_);
        return count_;
    }

    /**
     * @brief 持锁把计数直接设成指定初值，不发送条件变量通知。
     * @param initialValue 目标非负计数，调用者须满足后端范围。
     * @pre 准备阶段调用，保证没有并发等待、发送或其他重设操作。
     * @note 本实现没有负值/溢出检查，也不自动唤醒已等待线程。
     */
    void doInit(int initialValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        count_ = initialValue;
        DP_OSAL_LOGD("Semaphore initialized with value %d\n", initialValue);
    }
    mutable std::mutex mutex_;  ///< 保护 count_，const 查询也需持锁。
    std::condition_variable cond_;  ///< 令牌可用通知，与 mutex_ 和 count_ 谓词配合。
    int count_;  ///< 可用令牌数；由调用者避免负值初始量和累加溢出。
};

} // namespace dp::osal

#endif  // DP_OSAL_POSIX_SEMAPHORE_H_
