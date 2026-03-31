/** @file   test_power.cpp
 *  @brief  GTest suite for dp_hal PowerManageable CRTP mixin. */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_POWER_CPP_
#define TEST_POWER_CPP_

#include <gtest/gtest.h>

#include "dp_hal_power.h"
#include "dp_uart.h"
#include "mock_uart.h"

namespace {

/** @brief  Mock UART with power management support for testing the
 *          PowerManageable mixin combined with UartBase. */
class PowerMockUart : public dp::hal::UartBase<PowerMockUart>, public dp::hal::PowerManageable<PowerMockUart> {
    friend class dp::hal::UartBase<PowerMockUart>;
    friend class dp::hal::PowerManageable<PowerMockUart>;

public:
    static constexpr bool kSupportsDma = false;
    static constexpr size_t kDmaAlignment = 0;

    void reset() {
        power_state_ = dp::hal::PowerState::kActive;
        write_count_ = 0;
    }

    size_t writeCount() const { return write_count_; }

private:
    dp::hal::Status doConfigure(const dp::hal::UartConfig & /*cfg*/) { return dp::hal::Status::kOk; }

    dp::hal::Status doWrite(const uint8_t * /*buf*/, size_t /*len*/) {
        if (power_state_ == dp::hal::PowerState::kOff) {
            return dp::hal::Status::kError;
        }
        ++write_count_;
        return dp::hal::Status::kOk;
    }

    dp::hal::Status doRead(uint8_t * /*buf*/, size_t /*max_len*/, size_t *actual) {
        if (actual != nullptr) {
            *actual = 0;
        }
        return dp::hal::Status::kOk;
    }

    dp::hal::Status doFlush() { return dp::hal::Status::kOk; }

    dp::hal::Status doSetRxCallback(dp::hal::UartBase<PowerMockUart>::RxCallback /*cb*/, void * /*ctx*/) {
        return dp::hal::Status::kNotSupported;
    }

    dp::hal::Status doSetTxCompleteCallback(dp::hal::UartBase<PowerMockUart>::TxCompleteCallback /*cb*/,
                                            void * /*ctx*/) {
        return dp::hal::Status::kNotSupported;
    }

    dp::hal::Status doSetPowerState(dp::hal::PowerState state) {
        power_state_ = state;
        return dp::hal::Status::kOk;
    }

    dp::hal::PowerState doGetPowerState() { return power_state_; }

    dp::hal::PowerState power_state_ = dp::hal::PowerState::kActive;
    size_t write_count_ = 0;
};

class DpHalPowerTest : public ::testing::Test {
protected:
    void SetUp() override { uart_.reset(); }

    PowerMockUart uart_;
};

TEST_F(DpHalPowerTest, SetAndGetPowerState) {
    EXPECT_EQ(uart_.getPowerState(), dp::hal::PowerState::kActive);
    EXPECT_EQ(uart_.setPowerState(dp::hal::PowerState::kSleep), dp::hal::Status::kOk);
    EXPECT_EQ(uart_.getPowerState(), dp::hal::PowerState::kSleep);
    EXPECT_EQ(uart_.setPowerState(dp::hal::PowerState::kOff), dp::hal::Status::kOk);
    EXPECT_EQ(uart_.getPowerState(), dp::hal::PowerState::kOff);
}

TEST_F(DpHalPowerTest, WriteFailsWhenOff) {
    const uint8_t data[] = {0x01};

    // Write should succeed when active
    EXPECT_EQ(uart_.write(data, 1), dp::hal::Status::kOk);
    EXPECT_EQ(uart_.writeCount(), 1u);

    // Power off
    uart_.setPowerState(dp::hal::PowerState::kOff);

    // Write should fail when off
    EXPECT_EQ(uart_.write(data, 1), dp::hal::Status::kError);
    EXPECT_EQ(uart_.writeCount(), 1u);  // Count unchanged

    // Power back on
    uart_.setPowerState(dp::hal::PowerState::kActive);
    EXPECT_EQ(uart_.write(data, 1), dp::hal::Status::kOk);
    EXPECT_EQ(uart_.writeCount(), 2u);
}

}  // namespace

#endif  // TEST_POWER_CPP_
