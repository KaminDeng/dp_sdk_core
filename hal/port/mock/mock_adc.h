/** @file   mock_adc.h
 *  @brief  Mock ADC implementation for dp_hal host testing. */

#ifndef MOCK_ADC_H_
#define MOCK_ADC_H_

#include <map>
#include <vector>

#include "dp_adc.h"

namespace dp::hal::mock {

/** @brief  Record of an ADC configure call. */
struct AdcConfigRecord {
    uint8_t channel;          ///< Channel number.
    uint8_t resolution_bits;  ///< Resolution in bits.
};

/** @brief  Mock ADC for GTest verification. */
class MockAdc : public AdcBase<MockAdc> {
    friend class AdcBase<MockAdc>;

public:
    /** @brief  Reset all mock state. */
    void reset() {
        staged_values_.clear();
        config_records_.clear();
        read_channels_.clear();
    }

    /** @brief  Stage a value for a specific ADC channel.
     *  @param  channel  Channel number.
     *  @param  value    Value to return on read(). */
    void stageChannelValue(uint8_t channel, uint16_t value) { staged_values_[channel] = value; }

    /** @brief  Get configuration records. */
    const std::vector<AdcConfigRecord> &configRecords() const { return config_records_; }

    /** @brief  Get list of channels that were read. */
    const std::vector<uint8_t> &readChannels() const { return read_channels_; }

private:
    Status doConfigure(uint8_t channel, uint8_t resolution_bits) {
        config_records_.push_back({channel, resolution_bits});
        return Status::kOk;
    }

    uint16_t doRead(uint8_t channel) {
        read_channels_.push_back(channel);
        auto it = staged_values_.find(channel);
        if (it != staged_values_.end()) {
            return it->second;
        }
        return 0;
    }

    Status doStartContinuous(uint8_t /*channel*/, AdcBase::ConversionCallback /*cb*/, void * /*ctx*/) {
        return Status::kNotSupported;
    }

    Status doStopContinuous(uint8_t /*channel*/) { return Status::kNotSupported; }

    std::map<uint8_t, uint16_t> staged_values_;
    std::vector<AdcConfigRecord> config_records_;
    std::vector<uint8_t> read_channels_;
};

}  // namespace dp::hal::mock

#endif  // MOCK_ADC_H_
