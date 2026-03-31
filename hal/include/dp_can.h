/** @file   dp_can.h
 *  @brief  CAN bus CRTP interface for dp_hal.
 *
 *  Impl must provide:
 *    - Status doConfigure(const CanConfig& cfg)
 *    - Status doSend(const CanFrame& frame)
 *    - Status doReceive(CanFrame* frame)
 *    - Status doSetFilter(uint32_t id, uint32_t mask, bool is_extended)
 *    - Status doSetRxCallback(RxCallback cb, void* ctx) */

#ifndef DP_CAN_H_
#define DP_CAN_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  CAN bus interface (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class CanBase {
public:
    /** @brief  Configure the CAN peripheral.
     *  @param  cfg  CAN configuration parameters.
     *  @return Status::kOk on success. */
    Status configure(const CanConfig &cfg) { return impl().doConfigure(cfg); }

    /** @brief  Send a CAN frame.
     *  @param  frame  Frame to transmit.
     *  @return Status::kOk on success, kBusy if no TX mailbox available. */
    Status send(const CanFrame &frame) { return impl().doSend(frame); }

    /** @brief  Receive a CAN frame (non-blocking).
     *  @param  frame  Pointer to store the received frame.
     *  @return Status::kOk if a frame was available, kError if RX empty. */
    Status receive(CanFrame *frame) { return impl().doReceive(frame); }

    /** @brief  Configure a hardware acceptance filter.
     *  @param  id          Filter ID value.
     *  @param  mask        Filter mask (1 = must match, 0 = don't care).
     *  @param  is_extended True for 29-bit extended ID filter.
     *  @return Status::kOk on success. */
    Status setFilter(uint32_t id, uint32_t mask, bool is_extended) {
        return impl().doSetFilter(id, mask, is_extended);
    }

    /** @brief  Receive callback type.
     *  @param  frame  Reference to the received frame.
     *  @param  ctx    User context pointer. */
    using RxCallback = void (*)(const CanFrame &frame, void *ctx);

    /** @brief  Register a receive callback (called from ISR context).
     *  @param  cb   Callback function (nullptr to unregister).
     *  @param  ctx  User context pointer.
     *  @return Status::kOk on success. */
    Status setRxCallback(RxCallback cb, void *ctx) { return impl().doSetRxCallback(cb, ctx); }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::hal

#endif  // DP_CAN_H_
