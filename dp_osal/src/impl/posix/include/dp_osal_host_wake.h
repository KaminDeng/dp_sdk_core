/**
 * @file dp_osal_host_wake.h
 * @brief Linux主机计数唤醒后端，使用sem_post和CLOCK_MONOTONIC绝对期限等待。
 * @details 构造不初始化信号量，open后可提前signal保留令牌，wait消费一个令牌。
 * EINTR重试沿用同一期限，避免信号导致无限续期；close前须停止全部使用者。
 * 目录名POSIX不意味着其它平台已适配，当前文件显式要求Linux及sem_clockwait能力。
 */
#pragma once
#if !defined(__linux__)
#error "当前Host I/O后端要求Linux；其他平台必须提供显式实现"
#endif
#include "interface_host_wake.h"
#include <cerrno>
#include <ctime>
#include <semaphore.h>
namespace dp::osal {
/**
 * @brief 显式管理一个进程内匿名计数信号量的不可复制owner。
 * @note open/close生命周期不加锁，调用者协调；不能与仍在等待的线程并发销毁。
 */
class OSALHostWake : public HostWakeBase<OSALHostWake> {
    friend class HostWakeBase<OSALHostWake>;
public:
    /**
     * @brief 仅初始化内存字段，不调用sem_init。
     */
    OSALHostWake() noexcept = default;
    /**
     * @brief 尽力close并忽略返回值，使用者须已结束等待。
     */
    ~OSALHostWake() { (void)doClose(); }
    /**
     * @brief 禁止复制底层sem_t及打开状态。
     */
    OSALHostWake(const OSALHostWake&) = delete;
    /**
     * @brief 禁止复制赋值造成资源所有权混淆。
     */
    OSALHostWake& operator=(const OSALHostWake&) = delete;
private:
    /**
     * @brief 若未打开则sem_init为进程内、零初始令牌。
     * @return 初始化成功为true，重复open或系统错误为false。
     */
    bool doOpen() noexcept {
        if (opened_) return false;
        opened_ = ::sem_init(&semaphore_, 0, 0U) == 0;
        return opened_;
    }
    /**
     * @brief 已打开时尝试一次sem_post，令牌可供未来wait消费。
     * @return 已打开且发布成功为true，不表示等待线程已经运行。
     */
    bool doSignal() noexcept { return opened_ && ::sem_post(&semaphore_) == 0; }
    /**
     * @brief 用单调时钟构造相对毫秒期限，并在同一期限内重试被中断的等待。
     * @param milliseconds 相对毫秒等待量，零期限也可消费已经存在的令牌。
     * @return sem_clockwait成功为kSignaled，ETIMEDOUT或中断后已过期为kTimeout，其它失败kError。
     * @details 加秒/纳秒后规范化进位；EINTR后重新取单调now比较期限，不把每次重试改成新预算。
     * @note 消费令牌不证明业务条件已满足，醒来后仍需检查调用者自己的状态。
     */
    HostWakeResult doWaitForMs(uint32_t milliseconds) noexcept {
        if (!opened_) return HostWakeResult::kError;
        timespec deadline{};
        if (::clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return HostWakeResult::kError;
        deadline.tv_sec += static_cast<time_t>(milliseconds / 1000U);
        deadline.tv_nsec += static_cast<long>(milliseconds % 1000U) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) { ++deadline.tv_sec; deadline.tv_nsec -= 1000000000L; }
        for (;;) {
            const int result = ::sem_clockwait(&semaphore_, CLOCK_MONOTONIC, &deadline);
            if (result == 0) return HostWakeResult::kSignaled;
            if (errno == ETIMEDOUT) return HostWakeResult::kTimeout;
            if (errno != EINTR) return HostWakeResult::kError;
            timespec now{};
            if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) return HostWakeResult::kError;
            if (now.tv_sec > deadline.tv_sec ||
                (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) return HostWakeResult::kTimeout;
        }
    }
    /**
     * @brief 已打开时先清opened_再sem_destroy；未打开直接成功。
     * @return 销毁成功或原本关闭为true。
     * @pre 没有线程继续wait/signal；若销毁失败，本对象也不再标记打开，不会在析构自动重试。
     */
    bool doClose() noexcept {
        if (!opened_) return true;
        opened_ = false;
        return ::sem_destroy(&semaphore_) == 0;
    }
    sem_t semaphore_{}; ///< 进程内匿名信号量存储，只有open成功后才可使用。
    bool opened_ = false; ///< 生命周期标记，不是并发open/close的原子同步变量。
};
} // namespace dp::osal
