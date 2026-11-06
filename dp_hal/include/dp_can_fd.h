#ifndef DP_CAN_FD_H_
#define DP_CAN_FD_H_
#include "dp_hal_types.h"
#include <stdint.h>
namespace dp::hal {
/** @brief 独立FD帧；不包含或扩大经典CanFrame，不选FD时无该存储实例。 */
struct CanFdFrame {
    uint32_t id = 0;              /**< 11位或29位identifier；不含控制位。 */
    uint8_t dlc = 0;              /**< CAN FD编码DLC 0..15；不是任意字节数。 */
    bool extended = false;        /**< true选择29位ID，false选择11位ID。 */
    bool bit_rate_switch = false; /**< true请求数据相位切换到data_bitrate；不改变仲裁速率。 */
    uint8_t data[64]{};           /**< 固定上限64B，仅dlc映射的字节有效；FD不支持remote帧。 */
};
/** @brief FD总线部署参数；仲裁/数据相位位率独立，单位bit/s。 */
struct CanFdConfig {
    uint32_t arbitration_bitrate = 0; /**< 仲裁相位bit/s，必须非零。 */
    uint32_t data_bitrate = 0;        /**< 数据相位bit/s，独立配置且必须非零。 */
};
/** @brief FD能力；零slots表示Unsupported，不得据此解释诊断为健康。 */
struct CanFdCapabilities {
    uint8_t max_slots = 0;     /**< 实际可同时拥有的TX槽数量，不能硬编码邮箱数。 */
    uint8_t max_payload = 0;   /**< 实际FD数据字节上限，最多64。 */
    bool fd = false;           /**< 是否真正提供独立FD合同。 */
    bool rx = false;           /**< 是否已装配显式RX owned池。 */
    bool cancellation = false; /**< 是否接收取消请求；不等同取消完成或quiet。 */
};
/** @brief FD独立诊断值；bus_off/identity_fault及RX丢失均不能作为健康成功。 */
struct CanFdDiagnostics {
    uint32_t rx_overflow_count = 0;
    bool bus_off = false, identity_fault = false;
};
/** @brief 提交身份与payload按值复制；owner三word由调用方定义，不解释调用方业务数据。 */
struct CanFdTx {
    CanFdFrame frame; /**< submit返回前复制，调用方之后可释放输入。 */
    uint64_t owner[3]{};
};
/** @brief 硬件终态值；只有takeTerminal消费身份匹配终态后才能复用槽。 */
struct CanFdTerminal {
    uint64_t owner[3]{};
    uint32_t generation = 0; /**< 驱动槽预留代次，禁止回绕复用。 */
    uint8_t slot = 0;        /**< 实际槽号，不是单独足够的终态身份。 */
    bool sent = false;       /**< true为发送完成；false为失败/取消，均须消费才释放槽。 */
};
/** @brief 将编码DLC转字节数，非法值返回0xff。
 * @param dlc 编码DLC，合法范围0..15。
 * @return 0..64的合法长度或0xff错误标记；纯计算，不访问硬件。 */
constexpr uint8_t canFdLength(uint8_t dlc) noexcept {
    constexpr uint8_t table[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    return dlc < 16 ? table[dlc] : 0xff;
}
/** @brief 将精确合法FD字节长度转DLC，不隐式填充。
 * @param length 数据字节数。
 * @return 对应DLC或0xff（例如9、63均非法）。 */
constexpr uint8_t canFdDlc(uint8_t length) noexcept {
    for (uint8_t i = 0; i < 16; ++i)
        if (canFdLength(i) == length) return i;
    return 0xff;
}
template <class Impl>
class CanFdBase;
/** @brief FD一次性预留句柄，显式绑定实例/槽/代次；析构不取消。
 * @details 只允许移动构造，禁止复制/赋值覆盖未提交预留。必须由同一线程owner显式提交或取消。 */
template <class Impl>
class CanFdReservation {
public:
    CanFdReservation() noexcept = default;
    CanFdReservation(const CanFdReservation &) = delete;
    CanFdReservation &operator=(const CanFdReservation &) = delete;
    CanFdReservation &operator=(CanFdReservation &&) = delete;
    /** @brief 转移唯一预留，原句柄立即失效。@param other 同一owner域中的活动或空句柄。 */
    CanFdReservation(CanFdReservation &&other) noexcept
        : owner_(other.owner_), generation_(other.generation_), slot_(other.slot_) {
        other.owner_ = nullptr;
    }

private:
    Impl *owner_ = nullptr;
    uint32_t generation_ = 0;
    uint8_t slot_ = 0;
    friend class CanFdBase<Impl>;
};
/** @brief FD独立静态能力门面；不注册消费者、线程或隐藏队列。
 * @details 输入在submit返回前复制，硬件完成不释放槽，取消也必须等待真实终态消费。
 * 所有方法由同一owner串行调用；IRQ只负责平台驱动既有终态机制。 */
template <class Impl>
class CanFdBase {
public:
    using Reservation = CanFdReservation<Impl>;
    /** @brief 查询实际FD能力。@return 零slots意味着Unsupported。 */
    CanFdCapabilities capabilities() const noexcept { return impl().doCapabilities(); }
    /** @brief 配置独立两相位速率。@param c 单位bit/s，均须非零。@return 配置状态，不代表总线健康。 */
    Status configure(const CanFdConfig &c) noexcept {
        if (!c.arbitration_bitrate || !c.data_bitrate) return Status::kInvalidArg;
        return impl().doConfigure(c);
    }
    /** @brief 取得空槽预留。@param r 必须是无效句柄，失败保持原句柄。@return Busy为无可用槽。 */
    Status reserve(Reservation &r) noexcept {
        if (r.owner_) return Status::kInvalidArg;
        uint8_t slot = 0;        /**< 实际槽号，不是单独足够的终态身份。 */
        uint32_t generation = 0; /**< 驱动槽预留代次，禁止回绕复用。 */
        auto status = impl().doReserve(&slot, &generation);
        if (status == Status::kOk) {
            r.owner_ = &impl();
            r.slot_ = slot;
            r.generation_ = generation;
        }
        return status;
    }
    /** @brief 取消尚未提交的预留。@param r 同实例有效句柄。@return 成功后句柄失效，不取消已提交帧。 */
    Status cancelReservation(Reservation &r) noexcept {
        if (r.owner_ != &impl()) return Status::kInvalidArg;
        auto status = impl().doCancelReservation(r.slot_, r.generation_);
        if (status == Status::kOk) r.owner_ = nullptr;
        return status;
    }
    /** @brief 向原预留槽提交完整FD值，不回退任意空槽。
     * @param r 同实例一次性预留，进入driver前即消费，失败也不能重用。
     * @param tx 借用至返回；成功只表示复制接纳，不是发送/终态消费。
     * @return 非法ID/DLC为InvalidArg，不支持平台为NotSupported。 */
    Status submitReserved(Reservation &&r, const CanFdTx &tx) noexcept {
        if (r.owner_ != &impl()) return Status::kInvalidArg;
        const auto slot = r.slot_;
        const auto generation = r.generation_;
        r.owner_ = nullptr;
        return impl().doSubmitReserved(slot, generation, tx);
    }
    /** @brief 取走一个真实终态并释放其槽。@param out 调用方输出值，失败不改写。@return Busy表示暂无终态，非静默证明。
     */
    Status takeTerminal(CanFdTerminal *out) noexcept { return out ? impl().doTakeTerminal(out) : Status::kInvalidArg; }
    /** @brief 从唯一RX消费者取得一帧owned副本。@param out 调用方存储。@return Busy为空，NotSupported为未配置RX池。 */
    Status receive(CanFdFrame *out) noexcept { return out ? impl().doReceive(out) : Status::kInvalidArg; }
    /** @brief 读取FD本实例故障快照。@return 零值只有capabilities.fd为真时才有意义。 */
    CanFdDiagnostics diagnostics() const noexcept { return impl().doDiagnostics(); }
    /** @brief 对本独占物理实例所有在途槽请求取消。@return OK仍须逐一消费终态，不能当作quiet。 */
    Status requestCancelAll() noexcept { return impl().doRequestCancelAll(); }
    /** @brief 请求已提交槽取消。@param slot 实际能力范围内槽号。@return OK仅表示请求已接受；仍须消费终态。 */
    Status requestCancel(uint8_t slot) noexcept { return impl().doRequestCancel(slot); }

private:
    Impl &impl() noexcept { return *static_cast<Impl *>(this); }
    const Impl &impl() const noexcept { return *static_cast<const Impl *>(this); }
};
/** @brief 无FD平台的显式默认能力；不影响经典CanBase/CanAsyncBase。 */
class UnsupportedCanFd final : public CanFdBase<UnsupportedCanFd> {
    friend class CanFdBase<UnsupportedCanFd>;
    CanFdCapabilities doCapabilities() const noexcept { return {}; }
    Status doConfigure(const CanFdConfig &) noexcept { return Status::kNotSupported; }
    Status doReserve(uint8_t *, uint32_t *) noexcept { return Status::kNotSupported; }
    Status doCancelReservation(uint8_t, uint32_t) noexcept { return Status::kNotSupported; }
    Status doSubmitReserved(uint8_t, uint32_t, const CanFdTx &) noexcept { return Status::kNotSupported; }
    Status doTakeTerminal(CanFdTerminal *) noexcept { return Status::kNotSupported; }
    Status doRequestCancel(uint8_t) noexcept { return Status::kNotSupported; }
    Status doRequestCancelAll() noexcept { return Status::kNotSupported; }
    Status doReceive(CanFdFrame *) noexcept { return Status::kNotSupported; }
    CanFdDiagnostics doDiagnostics() const noexcept { return {}; }
};
}  // namespace dp::hal
#endif
