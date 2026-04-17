/** @file   mock_dac.h
 *  @brief  Mock DAC implementation for dp_hal host testing. */

#ifndef MOCK_DAC_H_
#define MOCK_DAC_H_

#include <vector>

#include "dp_dac.h"

namespace dp::hal::mock {

/** @brief  Record of a single DAC write. */
struct DacWriteRecord {
    uint8_t channel;  ///< Channel number.
    uint16_t value;   ///< Written value.
};

/** @brief  Record of a DAC configure call. */
struct DacConfigRecord {
    uint8_t channel;          ///< Channel number.
    uint8_t resolution_bits;  ///< Resolution in bits.
};

/** @brief  Mock DAC for GTest verification. */
class MockDac : public DacBase<MockDac> {
    friend class DacBase<MockDac>;

public:
    /** @brief  Reset all mock state. */
    void reset() {
        write_records_.clear();
        config_records_.clear();
    }

    /** @brief  Get all write records. */
    const std::vector<DacWriteRecord> &writeRecords() const { return write_records_; }

    /** @brief  Get configuration records. */
    const std::vector<DacConfigRecord> &configRecords() const { return config_records_; }

private:
    Status doConfigure(uint8_t channel, uint8_t resolution_bits) {
        config_records_.push_back({channel, resolution_bits});
        return Status::kOk;
    }

    Status doWrite(uint8_t channel, uint16_t value) {
        write_records_.push_back({channel, value});
        return Status::kOk;
    }

    std::vector<DacWriteRecord> write_records_;
    std::vector<DacConfigRecord> config_records_;
};

}  // namespace dp::hal::mock

#endif  // MOCK_DAC_H_
