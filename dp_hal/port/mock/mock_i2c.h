/** @file   mock_i2c.h
 *  @brief  Mock I2C bus implementation for dp_hal host testing. */

#ifndef MOCK_I2C_H_
#define MOCK_I2C_H_

#include <cstring>
#include <map>
#include <vector>

#include "dp_i2c.h"

namespace dp::hal::mock {

/** @brief  Record of a single I2C write operation. */
struct I2cWriteRecord {
    uint16_t addr;              ///< Slave address.
    std::vector<uint8_t> data;  ///< Written data.
};

/** @brief  Mock I2C bus for GTest verification. */
class MockI2cBus : public I2cBusBase<MockI2cBus> {
    friend class I2cBusBase<MockI2cBus>;

public:
    /** @brief  Reset all mock state. */
    void reset() {
        config_ = {};
        configured_ = false;
        write_records_.clear();
        staged_read_.clear();
    }

    /** @brief  Whether configure() was called. */
    bool wasConfigured() const { return configured_; }

    /** @brief  Get the last configuration applied. */
    const I2cConfig &lastConfig() const { return config_; }

    /** @brief  Get all write records. */
    const std::vector<I2cWriteRecord> &writeRecords() const { return write_records_; }

    /** @brief  Stage data for a specific slave address that read() returns.
     *  @param  addr  7-bit slave address.
     *  @param  data  Data bytes to stage.
     *  @param  len   Number of bytes. */
    void stageReadData(uint16_t addr, const uint8_t *data, size_t len) {
        auto &buf = staged_read_[addr];
        buf.insert(buf.end(), data, data + len);
    }

private:
    Status doConfigure(const I2cConfig &cfg) {
        config_ = cfg;
        configured_ = true;
        return Status::kOk;
    }

    Status doWrite(uint16_t addr, const uint8_t *buf, size_t len) {
        I2cWriteRecord rec;
        rec.addr = addr;
        if (buf != nullptr && len > 0) {
            rec.data.assign(buf, buf + len);
        }
        write_records_.push_back(rec);
        return Status::kOk;
    }

    Status doRead(uint16_t addr, uint8_t *buf, size_t len) {
        auto it = staged_read_.find(addr);
        if (it == staged_read_.end() || it->second.empty()) {
            std::memset(buf, 0, len);
            return Status::kOk;
        }
        auto &staged = it->second;
        size_t avail = len < staged.size() ? len : staged.size();
        std::memcpy(buf, staged.data(), avail);
        staged.erase(staged.begin(), staged.begin() + static_cast<ptrdiff_t>(avail));
        if (avail < len) {
            std::memset(buf + avail, 0, len - avail);
        }
        return Status::kOk;
    }

    Status doWriteRead(uint16_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) {
        Status s = doWrite(addr, tx, tx_len);
        if (s != Status::kOk) {
            return s;
        }
        return doRead(addr, rx, rx_len);
    }

    I2cConfig config_ = {};
    bool configured_ = false;
    std::vector<I2cWriteRecord> write_records_;
    std::map<uint16_t, std::vector<uint8_t>> staged_read_;
};

}  // namespace dp::hal::mock

#endif  // MOCK_I2C_H_
