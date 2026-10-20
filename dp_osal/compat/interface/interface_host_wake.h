/**
 * @file interface_host_wake.h
 * @brief 主机 owner/工作线程间的可计数唤醒接口，与通用 RTOS 信号量分开。
 * @details 仅转发显式 open/signal/wait/close；后端负责资源状态和单调期限等待。
 * 构造不取得资源，关闭前须确保没有并发等待或发送者继续访问，避免销毁使用中的资源。
 */
#pragma once
#if !DP_OSAL_ENABLE_LEGACY_COMPAT
#error "此旧接口仅由dp_osal_legacy_compat目标显式装配"
#endif

#include <cstdint>
namespace dp::osal {
/**
 * @brief 一次等待的结果：消耗唤醒令牌、期限已到或资源/系统错误。
 */
enum class HostWakeResult : uint8_t {
    kSignaled, ///< 已取得并消费一次唤醒令牌。
    kTimeout,  ///< 等待期限内未取得令牌。
    kError     ///< 资源未打开或后端操作失败。
};
/**
 * @brief 用 CRTP 绑定主机唤醒后端，无虚函数调度。
 * @tparam Impl 提供 doOpen/doSignal/doWaitForMs/doClose 的具体实现。
 */
template <typename Impl>
class HostWakeBase {
public:
    /**
     * @brief 显式初始化后端唤醒资源。
     * @return 成功时为 true；重复打开是否允许由后端规定，当前 Linux 实现拒绝重复 open。
     */
    bool open() noexcept { return impl().doOpen(); }
    /**
     * @brief 发布一次唤醒，已发布的令牌由后续等待消费。
     * @return 已打开且后端发布成功时为 true；不代表目标线程已经获得调度。
     */
    bool signal() noexcept { return impl().doSignal(); }
    /**
     * @brief 在相对毫秒期限内等待并消费一个唤醒令牌。
     * @param milliseconds 最多等待的相对毫秒数；零表示当前期限，不额外等待未来信号。
     * @return kSignaled、kTimeout 或 kError；中断重试与期限维护由后端负责。
     * @note 唤醒不等价于共享数据已满足条件，调用者应检查自己的业务谓词。
     */
    HostWakeResult wait_for_ms(uint32_t milliseconds) noexcept { return impl().doWaitForMs(milliseconds); }
    /**
     * @brief 释放已打开的唤醒资源；不能与资源使用并发。
     * @return 后端关闭成功或已关闭时为 true。
     */
    bool close() noexcept { return impl().doClose(); }
protected:
    /**
     * @brief 受保护的非虚析构，实际资源释放由后端析构或显式 close 完成。
     */
    ~HostWakeBase() = default;
private:
    /**
     * @brief 将接口基类静态转回具体后端。
     * @return 当前 Impl 对象引用。
     */
    Impl& impl() noexcept { return *static_cast<Impl*>(this); }
};
} // namespace dp::osal
