/**
 * @file interface_mutex.h
 * @brief 互斥锁 CRTP 接口，围绕获取操作记录同步测量。
 * @details 获取前后读取微秒计时并上报结果；记录的是整个后端调用耗时，
 * 不是纯调度延迟。unlock 不计时。锁所有权、递归能力和等待上下文由具体后端规定。
 */
#ifndef DP_OSAL_INTERFACE_MUTEX_H_
#define DP_OSAL_INTERFACE_MUTEX_H_

#include <cstdint>

#include "dp_osal_compat.h"
#include "dp_osal_measurement.h"

namespace dp::osal {

/** @brief 静态转发互斥操作的基类。
 * @tparam Impl 实现 doLock/doUnlock 等操作的后端，须授予本基类访问权限。 */
template <typename Impl>
class MutexBase {
public:
    /**
     * @brief 调用后端阻塞获取互斥锁，并记录调用时长及获取结果。
     * @return 后端报告成功时为 true；失败时调用者不能访问受保护数据。
     * @note 此层不检查递归获取或死锁，不能在不允许阻塞的上下文中调用。
     */
    DP_OSAL_HOT bool lock() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doLock();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kLock, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doLock();
    }
    /**
     * @brief 释放后端互斥锁，不纳入等待耗时统计。
     * @return 后端解锁结果；调用者须遵守锁归属及配对要求。
     */
    DP_OSAL_HOT bool unlock() { return impl().doUnlock(); }
    /**
     * @brief 尝试获取一次，不请求等待锁可用，并记录后端返回结果。
     * @return 成功获取时为 true；测量中的失败计数不能解读成所有实际竞争次数。
     */
    bool tryLock() {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryLock();
            detail::MeasurementAccess::recordWait(SynchronizationKind::kLock, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTryLock();
    }
    /**
     * @brief 按后端约定的相对超时获取锁，记录结果与总调用耗时。
     * @param timeout POSIX 当前按毫秒解释，CMSIS 直接传作内核 tick；本层不转换。
     * @return 在后端规则下成功获取时为 true，否则 false。
     * @note 不可跨后端盲目复用同一个数值作为相同物理时长。
     */
    DP_OSAL_COLD bool tryLockFor(uint32_t timeout) {
#if DP_OSAL_ENABLE_MEASUREMENT
        if (Measurement::enabled()) {
            const auto started = detail::measurementNow();
            const bool result = impl().doTryLockFor(timeout);
            detail::MeasurementAccess::recordWait(SynchronizationKind::kLock, started, detail::measurementNow(), result);
            return result;
        }
#endif
        return impl().doTryLockFor(timeout);
    }

protected:
    /**
     * @brief 受保护非虚析构，具体后端负责锁资源的释放。
     */
    ~MutexBase() = default;

private:
    /**
     * @brief 获取当前可写后端引用用于静态转发。
     * @return 当前 Impl 引用。
     */
    Impl &impl() { return *static_cast<Impl *>(this); }
    /**
     * @brief 获取当前只读后端引用。
     * @return 当前 Impl 的 const 引用。
     */
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

} // namespace dp::osal

#endif  // DP_OSAL_INTERFACE_MUTEX_H_
