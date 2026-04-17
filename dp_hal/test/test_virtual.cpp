/** @file   test_virtual.cpp
 *  @brief  GTest suite for dp_hal virtual wrappers (dependency injection). */

// NOLINTNEXTLINE(build/header_guard)
#ifndef TEST_VIRTUAL_CPP_
#define TEST_VIRTUAL_CPP_

#include <gtest/gtest.h>

#include "dp_adc.h"
#include "dp_can.h"
#include "dp_dac.h"
#include "dp_gpio.h"
#include "dp_hal_power.h"
#include "dp_hal_virtual.h"
#include "dp_i2c.h"
#include "dp_pwm.h"
#include "dp_spi.h"
#include "dp_timer.h"
#include "dp_uart.h"

namespace {

class VirtualFakeUart : public dp::hal::UartBase<VirtualFakeUart> {
    friend class dp::hal::UartBase<VirtualFakeUart>;

public:
    static constexpr bool kSupportsDma = false;
    static constexpr size_t kDmaAlignment = 0;

    int rx_cb_set_count = 0;
    int tx_cb_set_count = 0;
    int write_count = 0;
    size_t last_write_len = 0;

private:
    dp::hal::Status doConfigure(const dp::hal::UartConfig &) { return dp::hal::Status::kOk; }
    dp::hal::Status doWrite(const uint8_t *, size_t len) {
        ++write_count;
        last_write_len = len;
        return dp::hal::Status::kOk;
    }
    dp::hal::Status doRead(uint8_t *, size_t, size_t *actual) {
        if (actual != nullptr) {
            *actual = 0;
        }
        return dp::hal::Status::kOk;
    }
    dp::hal::Status doFlush() { return dp::hal::Status::kOk; }
    dp::hal::Status doSetRxCallback(dp::hal::UartBase<VirtualFakeUart>::RxCallback, void *) {
        ++rx_cb_set_count;
        return dp::hal::Status::kOk;
    }
    dp::hal::Status doSetTxCompleteCallback(dp::hal::UartBase<VirtualFakeUart>::TxCompleteCallback, void *) {
        ++tx_cb_set_count;
        return dp::hal::Status::kOk;
    }
};

class VirtualFakeGpio : public dp::hal::GpioPinBase<VirtualFakeGpio> {
    friend class dp::hal::GpioPinBase<VirtualFakeGpio>;

public:
    int irq_enable_count = 0;
    int irq_disable_count = 0;

private:
    dp::hal::Status doSetMode(dp::hal::PinMode) { return dp::hal::Status::kOk; }
    dp::hal::Status doWrite(dp::hal::PinState) { return dp::hal::Status::kOk; }
    dp::hal::PinState doRead() { return dp::hal::PinState::kActive; }
    dp::hal::Status doToggle() { return dp::hal::Status::kOk; }
    dp::hal::Status doEnableIrq(dp::hal::GpioIrqTrigger, dp::hal::GpioPinBase<VirtualFakeGpio>::IrqCallback, void *) {
        ++irq_enable_count;
        return dp::hal::Status::kOk;
    }
    dp::hal::Status doDisableIrq() {
        ++irq_disable_count;
        return dp::hal::Status::kOk;
    }
};

class VirtualFakeSpi : public dp::hal::SpiBusBase<VirtualFakeSpi> {
    friend class dp::hal::SpiBusBase<VirtualFakeSpi>;

public:
    static constexpr bool kSupportsDma = false;
    static constexpr size_t kDmaAlignment = 0;
    size_t last_transfer_len = 0;

private:
    dp::hal::Status doConfigure(const dp::hal::SpiConfig &) { return dp::hal::Status::kOk; }
    dp::hal::Status doTransfer(const uint8_t *, uint8_t *, size_t len) {
        last_transfer_len = len;
        return dp::hal::Status::kOk;
    }
    dp::hal::Status doWrite(const uint8_t *, size_t) { return dp::hal::Status::kOk; }
    dp::hal::Status doRead(uint8_t *, size_t) { return dp::hal::Status::kOk; }
    dp::hal::Status doTransferAsync(const uint8_t *, uint8_t *, size_t, dp::hal::SpiBusBase<VirtualFakeSpi>::TransferCompleteCallback, void *) {
        return dp::hal::Status::kOk;
    }
};

class VirtualFakeI2c : public dp::hal::I2cBusBase<VirtualFakeI2c> {
    friend class dp::hal::I2cBusBase<VirtualFakeI2c>;

private:
    dp::hal::Status doConfigure(const dp::hal::I2cConfig &) { return dp::hal::Status::kOk; }
    dp::hal::Status doWrite(uint16_t, const uint8_t *, size_t) { return dp::hal::Status::kOk; }
    dp::hal::Status doRead(uint16_t, uint8_t *, size_t) { return dp::hal::Status::kOk; }
    dp::hal::Status doWriteRead(uint16_t, const uint8_t *, size_t, uint8_t *, size_t) {
        return dp::hal::Status::kOk;
    }
};

class VirtualFakeAdc : public dp::hal::AdcBase<VirtualFakeAdc> {
    friend class dp::hal::AdcBase<VirtualFakeAdc>;

private:
    dp::hal::Status doConfigure(uint8_t, uint8_t) { return dp::hal::Status::kOk; }
    uint16_t doRead(uint8_t) { return 123u; }
    dp::hal::Status doStartContinuous(uint8_t, dp::hal::AdcBase<VirtualFakeAdc>::ConversionCallback, void *) {
        return dp::hal::Status::kOk;
    }
    dp::hal::Status doStopContinuous(uint8_t) { return dp::hal::Status::kOk; }
};

class VirtualFakeDac : public dp::hal::DacBase<VirtualFakeDac> {
    friend class dp::hal::DacBase<VirtualFakeDac>;

private:
    dp::hal::Status doConfigure(uint8_t, uint8_t) { return dp::hal::Status::kOk; }
    dp::hal::Status doWrite(uint8_t, uint16_t) { return dp::hal::Status::kOk; }
};

class VirtualFakeTimer : public dp::hal::TimerBase<VirtualFakeTimer> {
    friend class dp::hal::TimerBase<VirtualFakeTimer>;

private:
    dp::hal::Status doStart(uint32_t) { return dp::hal::Status::kOk; }
    dp::hal::Status doStop() { return dp::hal::Status::kOk; }
    dp::hal::Status doSetCallback(dp::hal::TimerBase<VirtualFakeTimer>::Callback, void *) { return dp::hal::Status::kOk; }
    uint32_t doGetCounterUs() { return 77u; }
};

class VirtualFakeCan : public dp::hal::CanBase<VirtualFakeCan> {
    friend class dp::hal::CanBase<VirtualFakeCan>;

public:
    int rx_cb_set_count = 0;

private:
    dp::hal::Status doConfigure(const dp::hal::CanConfig &) { return dp::hal::Status::kOk; }
    dp::hal::Status doSend(const dp::hal::CanFrame &) { return dp::hal::Status::kOk; }
    dp::hal::Status doReceive(dp::hal::CanFrame *) { return dp::hal::Status::kOk; }
    dp::hal::Status doSetFilter(uint32_t, uint32_t, bool) { return dp::hal::Status::kOk; }
    dp::hal::Status doSetRxCallback(dp::hal::CanBase<VirtualFakeCan>::RxCallback, void *) {
        ++rx_cb_set_count;
        return dp::hal::Status::kOk;
    }
};

class VirtualFakePwm : public dp::hal::PwmBase<VirtualFakePwm> {
    friend class dp::hal::PwmBase<VirtualFakePwm>;

private:
    dp::hal::Status doStart(uint32_t, float) { return dp::hal::Status::kOk; }
    dp::hal::Status doSetDuty(float) { return dp::hal::Status::kOk; }
    dp::hal::Status doSetFrequency(uint32_t) { return dp::hal::Status::kOk; }
    dp::hal::Status doStop() { return dp::hal::Status::kOk; }
};

class VirtualFakePower : public dp::hal::PowerManageable<VirtualFakePower> {
    friend class dp::hal::PowerManageable<VirtualFakePower>;

private:
    dp::hal::Status doSetPowerState(dp::hal::PowerState state) {
        state_ = state;
        return dp::hal::Status::kOk;
    }
    dp::hal::PowerState doGetPowerState() { return state_; }

    dp::hal::PowerState state_ = dp::hal::PowerState::kActive;
};

void NoopUartRx(const uint8_t *, size_t, void *) {}
void NoopDone(void *) {}
void NoopGpioIrq(void *) {}
void NoopCanRx(const dp::hal::CanFrame &, void *) {}

TEST(DpHalVirtualTest, UartVirtualForwardsAllMethodsIncludingCallbacks) {
    VirtualFakeUart fake;
    dp::hal::UartVirtual<VirtualFakeUart> v(fake);
    dp::hal::IUart &iu = v;
    const uint8_t b[2] = {0x11, 0x22};
    size_t actual = 0;
    dp::hal::UartConfig cfg{115200, 8, 1, 0};

    EXPECT_EQ(iu.configure(cfg), dp::hal::Status::kOk);
    EXPECT_EQ(iu.write(b, 2), dp::hal::Status::kOk);
    EXPECT_EQ(iu.read(nullptr, 0, &actual), dp::hal::Status::kOk);
    EXPECT_EQ(iu.flush(), dp::hal::Status::kOk);
    EXPECT_EQ(iu.setRxCallback(NoopUartRx, nullptr), dp::hal::Status::kOk);
    EXPECT_EQ(iu.setTxCompleteCallback(NoopDone, nullptr), dp::hal::Status::kOk);
    EXPECT_EQ(fake.last_write_len, 2u);
    EXPECT_EQ(fake.rx_cb_set_count, 1);
    EXPECT_EQ(fake.tx_cb_set_count, 1);
}

TEST(DpHalVirtualTest, GpioVirtualForwardsIrqMethods) {
    VirtualFakeGpio fake;
    dp::hal::GpioPinVirtual<VirtualFakeGpio> v(fake);
    dp::hal::IGpioPin &ig = v;

    EXPECT_EQ(ig.setMode(dp::hal::PinMode::kOutput), dp::hal::Status::kOk);
    EXPECT_EQ(ig.write(dp::hal::PinState::kActive), dp::hal::Status::kOk);
    EXPECT_EQ(ig.read(), dp::hal::PinState::kActive);
    EXPECT_EQ(ig.toggle(), dp::hal::Status::kOk);
    EXPECT_EQ(ig.enableIrq(dp::hal::GpioIrqTrigger::kRising, NoopGpioIrq, nullptr), dp::hal::Status::kOk);
    EXPECT_EQ(ig.disableIrq(), dp::hal::Status::kOk);
    EXPECT_EQ(fake.irq_enable_count, 1);
    EXPECT_EQ(fake.irq_disable_count, 1);
}

TEST(DpHalVirtualTest, SpiI2cAdcDacTimerVirtualForwardsBasicMethods) {
    VirtualFakeSpi spi;
    VirtualFakeI2c i2c;
    VirtualFakeAdc adc;
    VirtualFakeDac dac;
    VirtualFakeTimer timer;

    dp::hal::SpiBusVirtual<VirtualFakeSpi> vs(spi);
    dp::hal::I2cBusVirtual<VirtualFakeI2c> vi(i2c);
    dp::hal::AdcVirtual<VirtualFakeAdc> va(adc);
    dp::hal::DacVirtual<VirtualFakeDac> vd(dac);
    dp::hal::TimerVirtual<VirtualFakeTimer> vt(timer);

    dp::hal::SpiConfig scfg{1000000, 0, 0};
    dp::hal::I2cConfig icfg{400000};
    uint8_t buf[4] = {};

    EXPECT_EQ(vs.configure(scfg), dp::hal::Status::kOk);
    EXPECT_EQ(vs.transfer(buf, buf, 4), dp::hal::Status::kOk);
    EXPECT_EQ(vs.transferAsync(buf, buf, 4, NoopDone, nullptr), dp::hal::Status::kOk);
    EXPECT_EQ(vi.configure(icfg), dp::hal::Status::kOk);
    EXPECT_EQ(vi.write(0x50, buf, 4), dp::hal::Status::kOk);
    EXPECT_EQ(vi.read(0x50, buf, 4), dp::hal::Status::kOk);
    EXPECT_EQ(vi.writeRead(0x50, buf, 1, buf, 2), dp::hal::Status::kOk);
    EXPECT_EQ(va.configure(1, 12), dp::hal::Status::kOk);
    EXPECT_EQ(va.read(1), 123u);
    EXPECT_EQ(va.startContinuous(1, nullptr, nullptr), dp::hal::Status::kOk);
    EXPECT_EQ(va.stopContinuous(1), dp::hal::Status::kOk);
    EXPECT_EQ(vd.configure(1, 12), dp::hal::Status::kOk);
    EXPECT_EQ(vd.write(1, 256), dp::hal::Status::kOk);
    EXPECT_EQ(vt.start(1000), dp::hal::Status::kOk);
    EXPECT_EQ(vt.setCallback(NoopDone, nullptr), dp::hal::Status::kOk);
    EXPECT_EQ(vt.getCounterUs(), 77u);
    EXPECT_EQ(vt.stop(), dp::hal::Status::kOk);
    EXPECT_EQ(spi.last_transfer_len, 4u);
}

TEST(DpHalVirtualTest, CanPwmPowerVirtualForwardsMethods) {
    VirtualFakeCan can;
    VirtualFakePwm pwm;
    VirtualFakePower power;

    dp::hal::CanVirtual<VirtualFakeCan> vc(can);
    dp::hal::PwmVirtual<VirtualFakePwm> vp(pwm);
    dp::hal::PowerManageableVirtual<VirtualFakePower> vpow(power);

    dp::hal::CanConfig ccfg{500000, 0};
    dp::hal::CanFrame frame{};
    frame.id = 0x321;
    frame.dlc = 1;
    frame.data[0] = 0xAB;

    EXPECT_EQ(vc.configure(ccfg), dp::hal::Status::kOk);
    EXPECT_EQ(vc.send(frame), dp::hal::Status::kOk);
    EXPECT_EQ(vc.receive(&frame), dp::hal::Status::kOk);
    EXPECT_EQ(vc.setFilter(0x100, 0x7FF, false), dp::hal::Status::kOk);
    EXPECT_EQ(vc.setRxCallback(NoopCanRx, nullptr), dp::hal::Status::kOk);
    EXPECT_EQ(can.rx_cb_set_count, 1);

    EXPECT_EQ(vp.start(1000, 50.0f), dp::hal::Status::kOk);
    EXPECT_EQ(vp.setDuty(40.0f), dp::hal::Status::kOk);
    EXPECT_EQ(vp.setFrequency(2000), dp::hal::Status::kOk);
    EXPECT_EQ(vp.stop(), dp::hal::Status::kOk);

    EXPECT_EQ(vpow.setPowerState(dp::hal::PowerState::kSleep), dp::hal::Status::kOk);
    EXPECT_EQ(vpow.getPowerState(), dp::hal::PowerState::kSleep);
}

}  // namespace

#endif  // TEST_VIRTUAL_CPP_
