/** @file   mock_uart.h
 *  @brief  Mock UART implementation for dp_hal host testing. */

#ifndef MOCK_UART_H_
#define MOCK_UART_H_

#include <cstring>
#include <vector>

#include "dp_uart.h"

namespace dp::hal::mock {

/** @brief  Mock UART for GTest verification. */
class MockUart : public UartBase<MockUart> {
    friend class UartBase<MockUart>;

public:
    /** @brief  Whether this mock supports DMA (no). */
    static constexpr bool kSupportsDma = false;

    /** @brief  DMA alignment requirement (none). */
    static constexpr size_t kDmaAlignment = 0;

    /** @brief  Reset all mock state. */
    void reset() {
        written_.clear();
        staged_read_.clear();
        config_ = {};
        configured_ = false;
        flushed_ = false;
    }

    /** @brief  Get all bytes written via write(). */
    const std::vector<uint8_t> &written() const { return written_; }

    /** @brief  Get total number of bytes written. */
    size_t writtenBytes() const { return written_.size(); }

    /** @brief  Get the last configuration applied. */
    const UartConfig &lastConfig() const { return config_; }

    /** @brief  Whether configure() was called. */
    bool wasConfigured() const { return configured_; }

    /** @brief  Whether flush() was called. */
    bool wasFlushed() const { return flushed_; }

    /** @brief  Stage data that will be returned by read().
     *  @param  data  Data bytes to stage.
     *  @param  len   Number of bytes. */
    void stageReadData(const uint8_t *data, size_t len) { staged_read_.insert(staged_read_.end(), data, data + len); }

private:
    Status doConfigure(const UartConfig &cfg) {
        config_ = cfg;
        configured_ = true;
        return Status::kOk;
    }

    Status doWrite(const uint8_t *buf, size_t len) {
        written_.insert(written_.end(), buf, buf + len);
        return Status::kOk;
    }

    Status doRead(uint8_t *buf, size_t max_len, size_t *actual) {
        size_t avail = max_len < staged_read_.size() ? max_len : staged_read_.size();
        if (avail > 0) {
            std::memcpy(buf, staged_read_.data(), avail);
            staged_read_.erase(staged_read_.begin(), staged_read_.begin() + static_cast<ptrdiff_t>(avail));
        }
        if (actual != nullptr) {
            *actual = avail;
        }
        return Status::kOk;
    }

    Status doFlush() {
        flushed_ = true;
        return Status::kOk;
    }

    Status doSetRxCallback(UartBase::RxCallback /*cb*/, void * /*ctx*/) { return Status::kNotSupported; }

    Status doSetTxCompleteCallback(UartBase::TxCompleteCallback /*cb*/, void * /*ctx*/) {
        return Status::kNotSupported;
    }

    std::vector<uint8_t> written_;
    std::vector<uint8_t> staged_read_;
    UartConfig config_ = {};
    bool configured_ = false;
    bool flushed_ = false;
};

}  // namespace dp::hal::mock

#endif  // MOCK_UART_H_
