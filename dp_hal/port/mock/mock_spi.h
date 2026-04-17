/** @file   mock_spi.h
 *  @brief  Mock SPI bus implementation for dp_hal host testing. */

#ifndef MOCK_SPI_H_
#define MOCK_SPI_H_

#include <cstring>
#include <vector>

#include "dp_spi.h"

namespace dp::hal::mock {

/** @brief  Record of a single SPI transfer operation. */
struct SpiTransferRecord {
    std::vector<uint8_t> tx_data;  ///< Bytes transmitted.
    size_t len;                    ///< Transfer length.
};

/** @brief  Mock SPI bus for GTest verification. */
class MockSpiBus : public SpiBusBase<MockSpiBus> {
    friend class SpiBusBase<MockSpiBus>;

public:
    /** @brief  Whether this mock supports DMA (no). */
    static constexpr bool kSupportsDma = false;

    /** @brief  DMA alignment requirement (none). */
    static constexpr size_t kDmaAlignment = 0;

    /** @brief  Reset all mock state. */
    void reset() {
        config_ = {};
        configured_ = false;
        transfers_.clear();
        staged_rx_.clear();
    }

    /** @brief  Whether configure() was called. */
    bool wasConfigured() const { return configured_; }

    /** @brief  Get the last configuration applied. */
    const SpiConfig &lastConfig() const { return config_; }

    /** @brief  Get all transfer records. */
    const std::vector<SpiTransferRecord> &transfers() const { return transfers_; }

    /** @brief  Stage data that will be copied to rx buffer on transfer.
     *  @param  data  Data bytes to stage.
     *  @param  len   Number of bytes. */
    void stageRxData(const uint8_t *data, size_t len) { staged_rx_.insert(staged_rx_.end(), data, data + len); }

private:
    Status doConfigure(const SpiConfig &cfg) {
        config_ = cfg;
        configured_ = true;
        return Status::kOk;
    }

    Status doTransfer(const uint8_t *tx, uint8_t *rx, size_t len) {
        SpiTransferRecord rec;
        if (tx != nullptr) {
            rec.tx_data.assign(tx, tx + len);
        }
        rec.len = len;
        transfers_.push_back(rec);

        if (rx != nullptr) {
            size_t avail = len < staged_rx_.size() ? len : staged_rx_.size();
            if (avail > 0) {
                std::memcpy(rx, staged_rx_.data(), avail);
                staged_rx_.erase(staged_rx_.begin(), staged_rx_.begin() + static_cast<ptrdiff_t>(avail));
            }
            if (avail < len) {
                std::memset(rx + avail, 0, len - avail);
            }
        }
        return Status::kOk;
    }

    Status doWrite(const uint8_t *buf, size_t len) { return doTransfer(buf, nullptr, len); }

    Status doRead(uint8_t *buf, size_t len) { return doTransfer(nullptr, buf, len); }

    Status doTransferAsync(const uint8_t * /*tx*/, uint8_t * /*rx*/, size_t /*len*/,
                           SpiBusBase::TransferCompleteCallback /*cb*/, void * /*ctx*/) {
        return Status::kNotSupported;
    }

    SpiConfig config_ = {};
    bool configured_ = false;
    std::vector<SpiTransferRecord> transfers_;
    std::vector<uint8_t> staged_rx_;
};

}  // namespace dp::hal::mock

#endif  // MOCK_SPI_H_
