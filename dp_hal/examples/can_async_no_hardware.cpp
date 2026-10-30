/** @file can_async_no_hardware.cpp
 * @brief 非Dummy离线示例：旧CanBase发送与独立单槽异步能力。没有硬件/线程/后台队列。
 */
#include "dp_can.h"
#include "dp_can_async.h"
#include <cassert>
#include <utility>

using namespace dp::hal;

class BasicDevice final : public CanBase<BasicDevice> {
    friend class dp::hal::CanBase<BasicDevice>;
    Status doConfigure(const CanConfig&) { return Status::kOk; }
    Status doSend(const CanFrame& frame) { return can_async_detail::validateClassicFrame(frame); }
    Status doReceive(CanFrame*) { return Status::kNotSupported; }
    Status doSetFilter(uint32_t, uint32_t, bool) { return Status::kNotSupported; }
    Status doSetRxCallback(RxCallback, void*) { return Status::kNotSupported; }
};

// 明确的教学设备模型。完整的production port仍需寄存器/IRQ/错误与时间域实现。
class ExampleDevice final : public CanAsyncBase<ExampleDevice> {
    friend class dp::hal::CanAsyncBase<ExampleDevice>;

public:
    void simulate_completion(CanTerminalState state) noexcept {
        assert(state_ == 2U);
        terminal_ = state;
        state_ = 3U;
    }

private:
    CanAsyncCapabilities doCapabilities() const noexcept {
        return {1, 1, 8, 1, kCanAsyncReservedSubmit | kCanAsyncTerminal, 0};
    }
    Status doStart() noexcept {
        started_ = true;
        return Status::kOk;
    }
    Status doReserve(uint8_t* slot) noexcept {
        if (!started_) return Status::kError;
        if (state_) return Status::kBusy;
        state_ = 1;
        *slot = 0;
        return Status::kOk;
    }
    Status doCancelReservation(uint8_t slot) noexcept {
        if (slot != 0 || state_ != 1) return Status::kInvalidArg;
        state_ = 0;
        return Status::kOk;
    }
    Status doSubmitReserved(uint8_t slot, const CanAsyncTx& tx) noexcept {
        if (slot != 0 || state_ != 1) return Status::kInvalidArg;
        const auto valid = can_async_detail::validateClassicFrame(tx.frame);
        if (valid != Status::kOk) {
            state_ = 0;
            return valid;
        }
        if (tx.frame.is_extended || tx.frame.is_remote) {
            state_ = 0;
            return Status::kNotSupported;
        }
        value_ = tx;
        state_ = 2;
        return Status::kOk;
    }
    Status doSubmitDirect(const CanAsyncTx&) noexcept { return Status::kNotSupported; }
    Status doTakeTerminal(CanAsyncTerminal* out) noexcept {
        if (state_ != 3) return Status::kBusy;
        *out = {};
        out->owner[0] = value_.opaque[0];
        out->owner[1] = value_.opaque[1];
        out->owner[2] = value_.opaque[4];
        out->owner_byte = static_cast<uint8_t>(value_.opaque[6]);
        out->state = terminal_;
        out->slot = 0;
        state_ = 0;
        return Status::kOk;
    }
    Status doReceive(CanAsyncRx*) noexcept { return Status::kNotSupported; }
    CanCancelResult doRequestCancel(CanCancelSelection) noexcept { return {CanCancelState::kNotSupported, 0, 0}; }
    Status doRequestAbortAllBestEffort() noexcept { return Status::kNotSupported; }
    Status doSetWakeCallback(void (*)(void*), void*) noexcept { return Status::kNotSupported; }
    CanAsyncDiagnostics doDiagnostics() const noexcept { return {}; }
    // 不实现观察原点；显式拒绝，不伪造时间。普通能力仍能工作。
    Status doReserveObserved(uint8_t*) noexcept { return Status::kNotSupported; }
    Status doSubmitReservedObserved(uint8_t slot, const CanAsyncTx&, CanSubmitObservation* out) noexcept {
        out->valid = false;
        (void)doCancelReservation(slot);
        return Status::kNotSupported;
    }
    Status doSubmitDirectObserved(const CanAsyncTx&, CanSubmitObservation* out) noexcept {
        out->valid = false;
        return Status::kNotSupported;
    }
    Status doTakeTerminalObserved(CanAsyncTerminal*, uint32_t*) noexcept { return Status::kNotSupported; }
    CanCancelResult doRequestCancelObserved(CanCancelSelection, CanCancelObservation* out) noexcept {
        out->valid = false;
        return {CanCancelState::kNotSupported, 0, 0};
    }
    Status doObserveInFlight(CanInFlightObservation* out) noexcept {
        out->valid = false;
        return Status::kNotSupported;
    }
    CanAsyncTx value_{};
    uint8_t state_{};
    bool started_{};
    CanTerminalState terminal_{};
};

int main() {
    CanFrame heartbeat{};
    heartbeat.id = 0x321;
    heartbeat.dlc = 2;
    heartbeat.data[0] = 0x12;
    heartbeat.data[1] = 0x34;
    BasicDevice basic;
    assert(basic.configure({}) == Status::kOk);
    assert(basic.send(heartbeat) == Status::kOk);  // 模型接纳，不是wire发送证明。
    ExampleDevice async;
    constexpr uint32_t required = kCanAsyncReservedSubmit | kCanAsyncTerminal;
    assert((async.capabilities().features & required) == required && async.start() == Status::kOk);
    CanAsyncTx request{};
    request.frame = heartbeat;
    request.opaque[0] = 0x123456789abcdef0ULL;      // 调用者的请求标记。
    request.opaque[1] = 0x1122334455667788ULL;      // 调用者的会话标记。
    request.opaque[4] = (uint64_t(1) << 32U) | 2U;  // 调用者的代次/序号，HAL不分配。
    request.opaque[6] = 0xa5;                       // 设备模型回传的owner byte，不表示Dummy轴数。
    ExampleDevice::Reservation reservation;
    assert(async.reserve(reservation) == Status::kOk);
    assert(async.submitReserved(std::move(reservation), request) == Status::kOk);
    CanAsyncTerminal terminal{};
    assert(async.takeTerminal(&terminal) == Status::kBusy);
    async.simulate_completion(CanTerminalState::kSent);
    assert(async.takeTerminal(&terminal) == Status::kOk);
    assert(terminal.owner[0] == request.opaque[0] && terminal.owner[1] == request.opaque[1]);
    assert(terminal.owner[2] == request.opaque[4] && terminal.owner_byte == 0xa5);
    assert(terminal.state == CanTerminalState::kSent);
    assert(async.submitDirect(request) == Status::kNotSupported);
    // 所有断言通过后以正常退出码结束；不为教学模型引入日志/设备依赖。
}
