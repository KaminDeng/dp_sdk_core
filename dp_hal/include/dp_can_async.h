/** @file dp_can_async.h
 * @brief 经典 CAN 异步能力层；不改变 dp_can.h 的兼容接口。
 * @details 本层不拥有线程、队列或硬件状态。所有操作非阻塞且不隐式重试。
 * 一个物理实例只能有一个 owner；初始化、IRQ/任务交接和时钟域由 port 明确。
 * 输入值在提交返回前由实现复制；输出均为调用者拥有的值，不借用驱动槽地址。
 */
#ifndef DP_CAN_ASYNC_H_
#define DP_CAN_ASYNC_H_

#include <stdint.h>
#include "dp_hal_types.h"

namespace dp::hal {

enum class CanTerminalState : uint8_t { kSent, kAborted, kFailed };
enum class CanCancelState : uint8_t { kNoInFlight, kPending, kFailed, kNotSupported };

/** @brief features 位；缺失能力必须明确拒绝，不能退回另一发送路径。 */
enum CanAsyncFeature : uint32_t {
    kCanAsyncReservedSubmit = 1U << 0U,
    kCanAsyncDirectSubmit = 1U << 1U,
    kCanAsyncTerminal = 1U << 2U,
    kCanAsyncCapturedRx = 1U << 3U,
    kCanAsyncIsrWake = 1U << 4U,
    kCanAsyncClassCancellation = 1U << 5U,
    kCanAsyncBestEffortAbortAll = 1U << 6U,
    kCanAsyncOperationObservations = 1U << 7U,
};

struct CanAsyncTx {
    CanFrame frame;
    uint64_t opaque[7];  ///< 仅机械保存/回传，公共 HAL 不解释业务身份。
};
struct CanAsyncTerminal {
    uint64_t owner[3];
    uint32_t submitted_us, terminal_us, residence_us, native_error;
    uint8_t owner_byte, slot, transport_class;
    CanTerminalState state;
};
struct CanAsyncRx {
    CanFrame frame;
    uint64_t received_at_us;
    uint32_t measurement_ingress_us;
};
struct CanCancelSelection { uint8_t transport_class; };
struct CanCancelResult {
    CanCancelState state;
    uint32_t affected_slots;
    uint32_t native_status;
};
struct CanAsyncCapabilities {
    uint8_t max_in_flight, max_reservations, max_dlc, frame_flags;
    uint32_t features, cancel_class_mask;
};
/** @brief kError的锁存来源位；Overflow不与身份完整性错误或普通暂忙混淆。 */
enum CanAsyncFault : uint8_t {
    kCanAsyncFaultNone = 0U,
    kCanAsyncFaultEventOverflow = 1U << 0U,
    kCanAsyncFaultIdentityMismatch = 1U << 1U,
    kCanAsyncFaultBusOff = 1U << 2U,
};
struct CanAsyncDiagnostics {
    /// event_overflow_count保留平台历史共享故障总计，不能解释成纯环溢出次数。
    uint32_t event_depth, event_overflow_count, rx_overflow_count;
    uint32_t error_count, turnover, kick_count, last_terminal_tick_ms;
    bool bus_off, event_fault;
    uint8_t fault_flags;  ///< 原尾padding内，sizeof仍32；缺能力的全零值不代表健康。
};
/** @brief 本次操作原采样点；valid=false 时其余字段不得解释为新观察。 */
struct CanSubmitObservation {
    uint32_t submitted_native_us32;
    uint8_t slot;
    bool valid;
};
struct CanCancelObservation {
    uint32_t active_slots, submitted_slots, sampled_native_us32;
    uint8_t transport_class;
    bool valid;
};
struct CanInFlightObservation {
    uint32_t active_slots, submitted_slots, sampled_native_us32;
    uint8_t transport_class;
    bool valid;
};

template <class Impl> class CanAsyncBase;

/** @brief 单 owner、同物理实例/启动周期内的一次性预留句柄。
 * @details 只允许移动构造；析构不调用驱动。活动句柄必须显式提交或取消。
 * 禁止覆盖活动对象、跨 owner/重启保留，或在其地址 placement-new 绕过契约。
 */
template <class Impl>
class CanReservation {
public:
    CanReservation() noexcept = default;
    ~CanReservation() = default;
    CanReservation(const CanReservation&) = delete;
    CanReservation& operator=(const CanReservation&) = delete;
    CanReservation(CanReservation&& other) noexcept : slot_(other.slot_) { other.slot_ = 0xffU; }
    CanReservation& operator=(CanReservation&&) = delete;
private:
    uint8_t slot_ = 0xffU;
    friend class CanAsyncBase<Impl>;
};

/** @brief 静态派发的异步 CAN 能力；Impl 的 do* 必须为 private 并声明 friend。
 * @details submit 成功只表示硬件接纳；终态消费并验证身份后才允许复用槽。
 * 取消只请求硬件操作，Sent 可胜过取消；abort-all 的 kOk 不表示终态成功。
 * 时间字段按 port 声明的来源/位宽使用，不能跨 Host/HAL tick/TIM5/DWT 相减。
 */
template <class Impl>
class CanAsyncBase {
public:
    using Reservation = CanReservation<Impl>;
    CanAsyncCapabilities capabilities() const noexcept { return impl().doCapabilities(); }
    Status start() noexcept { return impl().doStart(); }
    Status reserve(Reservation& out) noexcept {
        if (out.slot_ != 0xffU) return Status::kInvalidArg;
        uint8_t selected = 0xffU;
        const Status result = impl().doReserve(&selected);
        if (result == Status::kOk) out.slot_ = selected;
        return result;
    }
    Status cancelReservation(Reservation& value) noexcept {
        if (value.slot_ == 0xffU) return Status::kInvalidArg;
        const Status result = impl().doCancelReservation(value.slot_);
        if (result == Status::kOk) value.slot_ = 0xffU;
        return result;
    }
    Status submitReserved(Reservation&& value, const CanAsyncTx& tx) noexcept {
        if (value.slot_ == 0xffU) return Status::kInvalidArg;
        const uint8_t slot = value.slot_;
        value.slot_ = 0xffU;
        return impl().doSubmitReserved(slot, tx);
    }
    Status submitDirect(const CanAsyncTx& tx) noexcept { return impl().doSubmitDirect(tx); }
    Status takeTerminal(CanAsyncTerminal* out) noexcept {
        return out == nullptr ? Status::kInvalidArg : impl().doTakeTerminal(out);
    }
    CanCancelResult requestCancel(CanCancelSelection selection) noexcept {
        return impl().doRequestCancel(selection);
    }
    Status requestAbortAllBestEffort() noexcept { return impl().doRequestAbortAllBestEffort(); }
    Status receive(CanAsyncRx* out) noexcept {
        return out == nullptr ? Status::kInvalidArg : impl().doReceive(out);
    }
    Status setWakeCallback(void (*callback)(void*), void* context) noexcept {
        return impl().doSetWakeCallback(callback, context);
    }
    CanAsyncDiagnostics diagnostics() const noexcept { return impl().doDiagnostics(); }
    /** @brief 可选逐调用诊断；不增加普通实现必须提供的 do*。 */
    Status reserveObserved(Reservation& out, uint8_t* logical_slot) noexcept {
        if (logical_slot == nullptr || out.slot_ != 0xffU) return Status::kInvalidArg;
        uint8_t selected = 0xffU;
        const Status result = impl().doReserveObserved(&selected);
        if (result == Status::kOk) { out.slot_ = selected; *logical_slot = selected; }
        return result;
    }
    Status submitReservedObserved(Reservation&& value, const CanAsyncTx& tx,
                                  CanSubmitObservation* observation) noexcept {
        if (observation == nullptr || value.slot_ == 0xffU) return Status::kInvalidArg;
        const uint8_t slot = value.slot_;
        value.slot_ = 0xffU;
        return impl().doSubmitReservedObserved(slot, tx, observation);
    }
    Status submitDirectObserved(const CanAsyncTx& tx, CanSubmitObservation* observation) noexcept {
        return observation == nullptr ? Status::kInvalidArg : impl().doSubmitDirectObserved(tx, observation);
    }
    Status takeTerminalObserved(CanAsyncTerminal* out, uint32_t* consumed_native_us32) noexcept {
        return out == nullptr || consumed_native_us32 == nullptr ? Status::kInvalidArg
            : impl().doTakeTerminalObserved(out, consumed_native_us32);
    }
    CanCancelResult requestCancelObserved(CanCancelSelection selection,
                                          CanCancelObservation* observation) noexcept {
        return observation == nullptr ? CanCancelResult{CanCancelState::kFailed, 0U, 0U}
            : impl().doRequestCancelObserved(selection, observation);
    }
    Status observeInFlight(CanInFlightObservation* out) noexcept {
        return out == nullptr ? Status::kInvalidArg : impl().doObserveInFlight(out);
    }
private:
    Impl& impl() noexcept { return *static_cast<Impl*>(this); }
    const Impl& impl() const noexcept { return *static_cast<const Impl*>(this); }
};

namespace can_async_detail {
/** @brief 经典帧自身的值约束；帧型是否支持仍由具体能力检查。 */
inline Status validateClassicFrame(const CanFrame& frame) noexcept {
    if (frame.dlc > 8U || frame.id > (frame.is_extended ? 0x1fffffffU : 0x7ffU))
        return Status::kInvalidArg;
    return Status::kOk;
}
}  // namespace can_async_detail

/** @brief 未实现异步能力的显式绑定；全零 diagnostics 不具有健康观测意义。 */
class UnsupportedCanAsync final : public CanAsyncBase<UnsupportedCanAsync> {
    friend class CanAsyncBase<UnsupportedCanAsync>;
private:
    CanAsyncCapabilities doCapabilities() const noexcept { return {}; }
    Status doStart() noexcept { return Status::kNotSupported; }
    Status doReserve(uint8_t*) noexcept { return Status::kNotSupported; }
    Status doCancelReservation(uint8_t) noexcept { return Status::kNotSupported; }
    Status doSubmitReserved(uint8_t, const CanAsyncTx& tx) noexcept { return doSubmitDirect(tx); }
    Status doSubmitDirect(const CanAsyncTx& tx) noexcept {
        const Status valid = can_async_detail::validateClassicFrame(tx.frame);
        return valid == Status::kOk ? Status::kNotSupported : valid;
    }
    Status doTakeTerminal(CanAsyncTerminal*) noexcept { return Status::kNotSupported; }
    CanCancelResult doRequestCancel(CanCancelSelection) noexcept {
        return {CanCancelState::kNotSupported, 0U, 0U};
    }
    Status doRequestAbortAllBestEffort() noexcept { return Status::kNotSupported; }
    Status doReceive(CanAsyncRx*) noexcept { return Status::kNotSupported; }
    Status doSetWakeCallback(void (*)(void*), void*) noexcept { return Status::kNotSupported; }
    CanAsyncDiagnostics doDiagnostics() const noexcept { return {}; }
    Status doReserveObserved(uint8_t*) noexcept { return Status::kNotSupported; }
    Status doSubmitReservedObserved(uint8_t, const CanAsyncTx& tx,
                                   CanSubmitObservation* observation) noexcept {
        return doSubmitDirectObserved(tx, observation);
    }
    Status doSubmitDirectObserved(const CanAsyncTx& tx, CanSubmitObservation* observation) noexcept {
        observation->valid = false;
        return doSubmitDirect(tx);
    }
    Status doTakeTerminalObserved(CanAsyncTerminal*, uint32_t*) noexcept { return Status::kNotSupported; }
    CanCancelResult doRequestCancelObserved(CanCancelSelection, CanCancelObservation* out) noexcept {
        out->valid = false;
        return {CanCancelState::kNotSupported, 0U, 0U};
    }
    Status doObserveInFlight(CanInFlightObservation* out) noexcept {
        out->valid = false;
        return Status::kNotSupported;
    }
};

}  // namespace dp::hal
#endif  // DP_CAN_ASYNC_H_
