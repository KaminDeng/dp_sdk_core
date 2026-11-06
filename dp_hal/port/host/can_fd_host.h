#pragma once
#include "dp_can_fd.h"
#include <cstring>
namespace dp::hal {
/** @brief 可选Host FD驱动合同模型，固定Slots个拥有槽；仅选FD时实例化。
 * @tparam Slots 显式容量1..32。硬件完成通过complete注入，不能冒充真实FD外设。
 * @details owner串行调用；state=3终态待消费仍占槽，不增加另一事件消费者或动态队列。 */
template <unsigned Slots>
class HostCanFd final : public CanFdBase<HostCanFd<Slots>> {
    friend class CanFdBase<HostCanFd<Slots>>;
    static_assert(Slots > 0 && Slots <= 32, "FD槽容量1..32");
    struct Slot {
        CanFdTx tx{};
        uint32_t generation = 0;
        uint8_t state = 0;
        bool canceled = false, sent = false;
    };
    Slot slots_[Slots]{};
    CanFdConfig config_{};
    CanFdFrame *rx_ = nullptr;
    unsigned rx_capacity_ = 0, rx_head_ = 0, rx_count_ = 0;
    uint32_t rx_overflow_ = 0;
    bool configured_ = false, bus_off_ = false, fault_ = false;

public:
    /** @brief 绑定调用方显式RX池；默认无RX能力，不分配隐藏队列。
     * @param rx 外部数组，寿命覆盖driver。@param capacity 数组帧数；单owner串行注入/消费，不能直接从ISR调用。 */
    explicit HostCanFd(CanFdFrame *rx = nullptr, unsigned capacity = 0) noexcept
        : rx_(rx), rx_capacity_(rx ? capacity : 0) {}
    HostCanFd(const HostCanFd &) = delete;
    HostCanFd &operator=(const HostCanFd &) = delete;
    /** @brief 读取已配置两相位速率。@return 单位bit/s，仅Host合同值，不证明物理FD服务时限。 */
    CanFdConfig configuration() const noexcept { return config_; }
    /** @brief 注入Host接收事件，按值保存；满池拒绝且粘滞计数，不覆盖旧帧。
     * @param frame ID/DLC合法完整值。@return Busy为满池，Error为bus-off/未启动，NotSupported为无池。 */
    Status injectRx(const CanFdFrame &frame) noexcept {
        if (!rx_capacity_) return Status::kNotSupported;
        if (!configured_ || bus_off_) return Status::kError;
        if (canFdLength(frame.dlc) == 0xff || frame.id > (frame.extended ? 0x1fffffffU : 0x7ffU))
            return Status::kInvalidArg;
        if (rx_count_ == rx_capacity_) {
            if (rx_overflow_ != UINT32_MAX) ++rx_overflow_;
            return Status::kBusy;
        }
        rx_[(rx_head_ + rx_count_) % rx_capacity_] = frame;
        ++rx_count_;
        return Status::kOk;
    }
    /** @brief Host线模型读取在途owned值，不释放或完成槽。
     * @param slot 槽号。@param out 输出副本。@return 非在途Busy，非法输出InvalidArg。 */
    Status pending(uint8_t slot, CanFdTx *out) const noexcept {
        if (slot >= Slots || !out) return Status::kInvalidArg;
        if (slots_[slot].state != 2) return Status::kBusy;
        *out = slots_[slot].tx;
        return Status::kOk;
    }
    /** @brief 注入完整硬件终态。@param slot 槽号。@param generation 预留代次。
     * @param owner 全3word身份，须与提交一致。@param sent Sent可胜过取消。
     * @return 错代/错身份拒绝且保留槽，不自动释放。 */
    Status complete(uint8_t slot, uint32_t generation, const uint64_t owner[3], bool sent) noexcept {
        if (slot >= Slots || !owner) return Status::kInvalidArg;
        auto &value = slots_[slot];
        if (value.state != 2 || generation != value.generation ||
            std::memcmp(owner, value.tx.owner, sizeof(value.tx.owner)))
            return Status::kInvalidArg;
        value.sent = sent;
        value.state = 3;
        return Status::kOk;
    }
    /** @brief 注入bus-off，后续提交必须失败；不清已有引用或终态。@param value 总线错误状态。 */
    void setBusOff(bool value) noexcept { bus_off_ = value; }
    /** @brief 查询某槽本地代次供Host驱动完成注入。@param slot 槽号。@return 非法槽为0。 */
    uint32_t generation(uint8_t slot) const noexcept { return slot < Slots ? slots_[slot].generation : 0; }

private:
    /** @brief 报告本实例显式槽数和RX池存在性。@return FD能力值；不代表真实硬件服务保证。 */
    CanFdCapabilities doCapabilities() const noexcept {
        return {static_cast<uint8_t>(Slots), 64, true, rx_capacity_ > 0, true};
    }
    /** @brief 在全部TX槽已消费、RX池为空时保存独立两相位速率。
     * @param config 已由公共门面校验的bit/s参数。@return Busy保留旧配置；OK清模型bus-off，不清代次耗尽故障。 */
    Status doConfigure(const CanFdConfig &config) noexcept {
        if (rx_count_) return Status::kBusy;
        for (const auto &slot : slots_)
            if (slot.state) return Status::kBusy;
        config_ = config;
        configured_ = true;
        bus_off_ = false;
        return Status::kOk;
    }
    /** @brief 在固定槽表中找到free槽并提升代次，绝不使用硬件完成但未消费的槽。
     * @param out 成功写入实际槽号。@param generation 成功写入非零本地槽代次。
     * @return Busy为容量耗尽；Error为未配置、bus-off或代次耗尽，不覆盖输出。 */
    Status doReserve(uint8_t *out, uint32_t *generation) noexcept {
        if (!configured_ || bus_off_ || fault_) return Status::kError;
        for (unsigned i = 0; i < Slots; ++i)
            if (slots_[i].state == 0) {
                auto &slot = slots_[i];
                if (slot.generation == UINT32_MAX) {
                    fault_ = true;
                    return Status::kError;
                }
                ++slot.generation;
                slot.state = 1;
                slot.canceled = false;
                *out = static_cast<uint8_t>(i);
                *generation = slot.generation;
                return Status::kOk;
            }
        return Status::kBusy;
    }
    /** @brief 仅释放身份匹配且尚未提交的预留。@param index 实际槽号。
     * @param generation 该预留代次。@return InvalidArg拒绝旧句柄，OK令槽free。 */
    Status doCancelReservation(uint8_t index, uint32_t generation) noexcept {
        if (index >= Slots || slots_[index].state != 1 || slots_[index].generation != generation)
            return Status::kInvalidArg;
        slots_[index].state = 0;
        return Status::kOk;
    }
    /** @brief 消费已预留槽并复制FD值；格式/总线故障时释放未提交预留。
     * @param index 原预留槽号。@param generation 原预留代次。@param tx 借用至返回的完整FD值和owner身份。
     * @return OK只表示state=2接纳；随后必须由complete注入真实模型终态并消费。 */
    Status doSubmitReserved(uint8_t index, uint32_t generation, const CanFdTx &tx) noexcept {
        if (index >= Slots || slots_[index].state != 1 || slots_[index].generation != generation)
            return Status::kInvalidArg;
        auto &slot = slots_[index];
        slot.state = 0;
        if (canFdLength(tx.frame.dlc) == 0xff || tx.frame.id > (tx.frame.extended ? 0x1fffffffU : 0x7ffU))
            return Status::kInvalidArg;
        if (bus_off_ || fault_) return Status::kError;
        slot.tx = tx;
        slot.state = 2;
        return Status::kOk;
    }
    /** @brief 取一个state=3终态并在返回前释放原槽。
     * @param out 调用方输出，成功拥有3word身份/代次/槽和发送结果。
     * @return Busy为空，Error为粘滞身份资源故障；失败不改输出。 */
    Status doTakeTerminal(CanFdTerminal *out) noexcept {
        for (unsigned i = 0; i < Slots; ++i)
            if (slots_[i].state == 3) {
                auto &slot = slots_[i];
                CanFdTerminal result{};
                std::memcpy(result.owner, slot.tx.owner, sizeof(result.owner));
                result.generation = slot.generation;
                result.slot = static_cast<uint8_t>(i);
                result.sent = slot.sent;
                *out = result;
                slot.state = 0;
                return Status::kOk;
            }
        return fault_ ? Status::kError : Status::kBusy;
    }
    /** @brief 仅唯一owner消费RX owned值；错误/空池均不改输出。
     * @param out 接收一帧的owned输出。@return 无池NotSupported，空池Busy，有帧OK。 */
    Status doReceive(CanFdFrame *out) noexcept {
        if (!rx_capacity_) return Status::kNotSupported;
        if (!rx_count_) return Status::kBusy;
        *out = rx_[rx_head_];
        rx_head_ = (rx_head_ + 1) % rx_capacity_;
        --rx_count_;
        return Status::kOk;
    }
    /** @brief 读取本实例bus-off、代次资源故障和粘滞RX丢失计数。@return owned快照，不清故障。 */
    CanFdDiagnostics doDiagnostics() const noexcept { return {rx_overflow_, bus_off_, fault_}; }
    /** @brief 取消请求不变更槽终态；模型必须随后注入硬件Sent/Aborted。
     * @return OK只表示所有在途槽已标记，不释放引用、不合成终态。 */
    Status doRequestCancelAll() noexcept {
        for (auto &slot : slots_)
            if (slot.state == 2) slot.canceled = true;
        return Status::kOk;
    }
    /** @brief 标记指定在途槽取消请求，保留payload和身份直到complete后消费。
     * @param index 实际槽号。@return InvalidArg为非在途槽；OK不表示硬件已静默。 */
    Status doRequestCancel(uint8_t index) noexcept {
        if (index >= Slots || slots_[index].state != 2) return Status::kInvalidArg;
        slots_[index].canceled = true;
        return Status::kOk; /* 等真实complete，Sent可胜过取消。 */
    }
};
}  // namespace dp::hal
