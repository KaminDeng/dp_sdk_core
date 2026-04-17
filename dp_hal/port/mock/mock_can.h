/** @file   mock_can.h
 *  @brief  Mock CAN bus implementation for dp_hal host testing. */

#ifndef MOCK_CAN_H_
#define MOCK_CAN_H_

#include <vector>

#include "dp_can.h"

namespace dp::hal::mock {

/** @brief  Mock CAN bus for GTest verification. */
class MockCan : public CanBase<MockCan> {
    friend class CanBase<MockCan>;

public:
    /** @brief  Reset all mock state. */
    void reset() {
        tx_frames_.clear();
        rx_frames_.clear();
        configured_ = false;
    }

    /** @brief  Whether configure() was called. */
    bool isConfigured() const { return configured_; }

    /** @brief  Get all transmitted frames. */
    const std::vector<CanFrame> &txFrames() const { return tx_frames_; }

    /** @brief  Stage a frame for the next receive() call. */
    void stageRxFrame(const CanFrame &frame) { rx_frames_.push_back(frame); }

private:
    Status doConfigure(const CanConfig & /*cfg*/) {
        configured_ = true;
        return Status::kOk;
    }

    Status doSend(const CanFrame &frame) {
        tx_frames_.push_back(frame);
        return Status::kOk;
    }

    Status doReceive(CanFrame *frame) {
        if (rx_frames_.empty()) {
            return Status::kError;
        }
        *frame = rx_frames_.front();
        rx_frames_.erase(rx_frames_.begin());
        return Status::kOk;
    }

    Status doSetFilter(uint32_t /*id*/, uint32_t /*mask*/, bool /*is_extended*/) {
        return Status::kOk;
    }

    Status doSetRxCallback(CanBase::RxCallback /*cb*/, void * /*ctx*/) {
        return Status::kNotSupported;
    }

    bool configured_ = false;
    std::vector<CanFrame> tx_frames_;
    std::vector<CanFrame> rx_frames_;
};

}  // namespace dp::hal::mock

#endif  // MOCK_CAN_H_
