/**
 * @file dp_osal_static_timer.h
 * @brief 由owner主动派发、借用外部存储的一次性定时器，不创建后台线程或OS定时器。
 * @details owner先init绑定存储与回调，再arm_at设绝对tick，循环中调用dispatch_due检查到期。
 * 所有方法在同一个执行线程使用，不支持并发或ISR调用；存储和回调上下文须覆盖使用期。
 * tick单位由调用者定义，arm与dispatch必须同域，先后判断仅在小于2^31 tick的窗口内有效。
 */
#pragma once
#if !DP_OSAL_ENABLE_LEGACY_COMPAT
#error "此旧接口仅由dp_osal_legacy_compat目标显式装配"
#endif


#include <cstdint>

namespace dp::osal {

/**
 * @brief 到期后同步调用的原始函数指针，参数为init借用的上下文。
 */
using StaticTimerCallback = void (*)(void*);

/**
 * @brief 显式区分参数、初始化状态、布防状态与尚未到期。
 */
enum class StaticTimerStatus : uint8_t {
    kOk = 0,             ///< 当前操作完成，派发路径表示回调已返回。
    kInvalidArgument,    ///< 初始化回调为空。
    kAlreadyInitialized, ///< 定时器已有效或存储已被占用。
    kNotInitialized,     ///< 尚未绑定有效状态槽。
    kNotArmed,           ///< 已初始化但当前没有待派发期限。
    kNotDue,             ///< 在半圈比较约定下当前tick尚早于期限。
};

/**
 * @brief 调用者持有的固定状态槽，一次只能被一个StaticTimer借用。
 * @note 必须比借用它的定时器存活更久，不能在仍被使用时销毁或跨线程重设。
 */
class StaticTimerStorage {
public:
    /**
     * @brief 以空回调、未占用及未布防状态构造存储。
     */
    StaticTimerStorage() = default;
    /**
     * @brief 禁止复制状态槽，避免复制in_use后产生伪所有权。
     */
    StaticTimerStorage(const StaticTimerStorage&) = delete;
    /**
     * @brief 禁止覆盖一个可能正在被借用的状态槽。
     */
    StaticTimerStorage& operator=(const StaticTimerStorage&) = delete;

private:
    StaticTimerCallback callback = nullptr; ///< 借用的同步回调，init后有效。
    void* context = nullptr; ///< 原样传入回调的借用上下文。
    uint32_t deadline = 0U; ///< 调用者时钟域内的绝对tick。
    bool in_use = false; ///< 是否被某个StaticTimer占用，不是线程锁。
    bool armed = false; ///< 是否有待派发的一次性期限。

    friend class StaticTimer;
};

/**
 * @brief 单owner、不可复制或移动的外部存储句柄，析构时归还状态槽。
 */
class StaticTimer {
public:
    /**
     * @brief 构造未初始化句柄，不分配状态槽或启动任务。
     */
    StaticTimer() = default;
    /**
     * @brief 禁止复制借用句柄，避免多个对象释放同一存储。
     */
    StaticTimer(const StaticTimer&) = delete;
    /**
     * @brief 禁止复制赋值。
     */
    StaticTimer& operator=(const StaticTimer&) = delete;
    /**
     * @brief 禁止移动，保持定时器地址和借用关系稳定。
     */
    StaticTimer(StaticTimer&&) = delete;
    /**
     * @brief 禁止移动赋值。
     */
    StaticTimer& operator=(StaticTimer&&) = delete;
    /**
     * @brief 调用reset清空并归还借用存储；存储此时必须仍有效。
     */
    ~StaticTimer() { reset(); }

    /**
     * @brief 绑定一个未被占用的存储槽及回调，初始保持未布防。
     * @param storage 调用者提供且比本定时器存活更久的状态槽。
     * @param callback 非空的同步到期回调。
     * @param context 借用的回调上下文，允许NULL但须满足回调自身约定。
     * @return 空回调为kInvalidArgument；当前已有效或槽被占用为kAlreadyInitialized；否则kOk。
     * @note 回调有效性先于生命周期检查，失败不会抢占其它定时器的存储。
     */
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

    /**
     * @brief 写入绝对tick并布防，已有期限会被新值覆盖。
     * @param absolute_tick 与后续now_tick同域的32位绝对期限。
     * @return 未初始化为kNotInitialized，否则kOk。
     * @note 本函数不读时钟、不拒绝过去值，也不自动安排OS唤醒。
     */
    StaticTimerStatus arm_at(uint32_t absolute_tick) {
        if (!valid()) {
            return StaticTimerStatus::kNotInitialized;
        }
        storage_->deadline = absolute_tick;
        storage_->armed = true;
        return StaticTimerStatus::kOk;
    }

    /**
     * @brief 清除布防标记，保留回调及存储绑定。
     * @return 未初始化为kNotInitialized，否则kOk；未布防时重复cancel仍为kOk。
     */
    StaticTimerStatus cancel() {
        if (!valid()) {
            return StaticTimerStatus::kNotInitialized;
        }
        storage_->armed = false;
        return StaticTimerStatus::kOk;
    }

    /**
     * @brief owner检查到期并同步派发一次回调，派发前先清除布防标记。
     * @param now_tick 当前32位tick，单位及时间域与arm_at一致。
     * @return 未初始化、未布防、未到期分别有独立状态；派发后返回kOk。
     * @details 通过now-deadline的无符号差判断，差为零即到期；非零且不小于半圈视为尚未到期。
     * 调用前缓存回调和上下文、先解除布防，使回调能显式再次arm_at并保留新的布防状态。
     * @note 不捕获回调异常，也不限制回调耗时；owner负责回调适合当前执行上下文。
     */
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
        // 回调可以重布防；先缓存入口并清armed，返回后不再覆盖它设置的新状态。
        StaticTimerCallback callback = storage_->callback;
        void* context = storage_->context;
        storage_->armed = false;
        callback(context);
        return StaticTimerStatus::kOk;
    }

    /**
     * @brief 检查本对象是否绑定仍标记占用的状态槽。
     * @return 指针非空且in_use为true时有效，不代表已布防或到期。
     */
    bool valid() const { return storage_ != nullptr && storage_->in_use; }
    /**
     * @brief 检查有效状态槽是否已布防。
     * @return 有效且armed为true；不会读取时钟或派发回调。
     */
    bool armed() const { return valid() && storage_->armed; }

    /**
     * @brief 清空所借用槽的回调、上下文、期限及状态，并断开指针。
     * @note 可重复调用；不调用回调、不释放调用者的存储或上下文内存。
     */
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
    StaticTimerStorage* storage_ = nullptr; ///< 借用的状态槽；reset仅归还槽，不delete。
};

}  // namespace dp::osal
