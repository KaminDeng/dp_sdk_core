/** @file   dp_hal_virtual.h
 *  @brief  Optional virtual interface wrappers for dp_hal (dependency injection
 *          / GMock support).
 *
 *  These wrappers add a vtable indirection on top of the zero-cost CRTP
 *  interfaces. Use only in host tests where GMock is needed; production code
 *  should use the CRTP interfaces directly. */

#ifndef DP_HAL_VIRTUAL_H_
#define DP_HAL_VIRTUAL_H_

#include "dp_hal_types.h"

namespace dp::hal {

/* ---- UART virtual interface -------------------------------------------- */

/** @brief  Virtual UART interface for dependency injection / GMock. */
class IUart {
public:
    virtual ~IUart() = default;
    virtual Status configure(const UartConfig &cfg) = 0;
    virtual Status write(const uint8_t *buf, size_t len) = 0;
    virtual Status read(uint8_t *buf, size_t max_len, size_t *actual) = 0;
    virtual Status flush() = 0;
    virtual Status setRxCallback(void (*cb)(const uint8_t *data, size_t len, void *ctx), void *ctx) = 0;
    virtual Status setTxCompleteCallback(void (*cb)(void *ctx), void *ctx) = 0;
};

/** @brief  Wrap any CRTP UartBase implementation as IUart.
 *  @tparam Impl  Concrete UartBase implementation. */
template <typename Impl>
class UartVirtual : public IUart {
public:
    /** @brief  Construct from a CRTP implementation reference.
     *  @param  impl  Reference to the concrete UART implementation. */
    explicit UartVirtual(Impl &impl) : impl_(impl) {}

    Status configure(const UartConfig &cfg) override { return impl_.configure(cfg); }
    Status write(const uint8_t *buf, size_t len) override { return impl_.write(buf, len); }
    Status read(uint8_t *buf, size_t max_len, size_t *actual) override { return impl_.read(buf, max_len, actual); }
    Status flush() override { return impl_.flush(); }
    Status setRxCallback(void (*cb)(const uint8_t *data, size_t len, void *ctx), void *ctx) override {
        return impl_.setRxCallback(cb, ctx);
    }
    Status setTxCompleteCallback(void (*cb)(void *ctx), void *ctx) override {
        return impl_.setTxCompleteCallback(cb, ctx);
    }

private:
    Impl &impl_;
};

/* ---- GPIO virtual interface -------------------------------------------- */

/** @brief  Virtual GPIO interface for dependency injection / GMock. */
class IGpioPin {
public:
    virtual ~IGpioPin() = default;
    virtual Status setMode(PinMode mode) = 0;
    virtual Status write(PinState state) = 0;
    virtual PinState read() = 0;
    virtual Status toggle() = 0;
    virtual Status enableIrq(GpioIrqTrigger trigger, void (*cb)(void *ctx), void *ctx) = 0;
    virtual Status disableIrq() = 0;
};

/** @brief  Wrap any CRTP GpioPinBase implementation as IGpioPin.
 *  @tparam Impl  Concrete GpioPinBase implementation. */
template <typename Impl>
class GpioPinVirtual : public IGpioPin {
public:
    /** @brief  Construct from a CRTP implementation reference.
     *  @param  impl  Reference to the concrete GPIO implementation. */
    explicit GpioPinVirtual(Impl &impl) : impl_(impl) {}

    Status setMode(PinMode mode) override { return impl_.setMode(mode); }
    Status write(PinState state) override { return impl_.write(state); }
    PinState read() override { return impl_.read(); }
    Status toggle() override { return impl_.toggle(); }
    Status enableIrq(GpioIrqTrigger trigger, void (*cb)(void *ctx), void *ctx) override {
        return impl_.enableIrq(trigger, cb, ctx);
    }
    Status disableIrq() override { return impl_.disableIrq(); }

private:
    Impl &impl_;
};

/* ---- SPI virtual interface ----------------------------------------------- */

/** @brief  Asynchronous SPI transfer complete callback type.
 *  @param  ctx  User context pointer. */
using SpiTransferCompleteCallback = void (*)(void *ctx);

/** @brief  Virtual SPI bus interface for dependency injection / GMock. */
class ISpi {
public:
    virtual ~ISpi() = default;
    virtual Status configure(const SpiConfig &cfg) = 0;
    virtual Status transfer(const uint8_t *tx, uint8_t *rx, size_t len) = 0;
    virtual Status write(const uint8_t *buf, size_t len) = 0;
    virtual Status read(uint8_t *buf, size_t len) = 0;
    virtual Status transferAsync(const uint8_t *tx, uint8_t *rx, size_t len, SpiTransferCompleteCallback cb,
                                 void *ctx) = 0;
};

/** @brief  Wrap any CRTP SpiBusBase implementation as ISpi.
 *  @tparam Impl  Concrete SpiBusBase implementation. */
template <typename Impl>
class SpiBusVirtual : public ISpi {
public:
    /** @brief  Construct from a CRTP implementation reference.
     *  @param  impl  Reference to the concrete SPI bus implementation. */
    explicit SpiBusVirtual(Impl &impl) : impl_(impl) {}

    Status configure(const SpiConfig &cfg) override { return impl_.configure(cfg); }
    Status transfer(const uint8_t *tx, uint8_t *rx, size_t len) override { return impl_.transfer(tx, rx, len); }
    Status write(const uint8_t *buf, size_t len) override { return impl_.write(buf, len); }
    Status read(uint8_t *buf, size_t len) override { return impl_.read(buf, len); }
    Status transferAsync(const uint8_t *tx, uint8_t *rx, size_t len, SpiTransferCompleteCallback cb,
                         void *ctx) override {
        return impl_.transferAsync(tx, rx, len, cb, ctx);
    }

private:
    Impl &impl_;
};

/* ---- I2C virtual interface ----------------------------------------------- */

/** @brief  Virtual I2C bus interface for dependency injection / GMock. */
class II2c {
public:
    virtual ~II2c() = default;
    virtual Status configure(const I2cConfig &cfg) = 0;
    virtual Status write(uint16_t addr, const uint8_t *buf, size_t len) = 0;
    virtual Status read(uint16_t addr, uint8_t *buf, size_t len) = 0;
    virtual Status writeRead(uint16_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) = 0;
};

/** @brief  Wrap any CRTP I2cBusBase implementation as II2c.
 *  @tparam Impl  Concrete I2cBusBase implementation. */
template <typename Impl>
class I2cBusVirtual : public II2c {
public:
    /** @brief  Construct from a CRTP implementation reference.
     *  @param  impl  Reference to the concrete I2C bus implementation. */
    explicit I2cBusVirtual(Impl &impl) : impl_(impl) {}

    Status configure(const I2cConfig &cfg) override { return impl_.configure(cfg); }
    Status write(uint16_t addr, const uint8_t *buf, size_t len) override { return impl_.write(addr, buf, len); }
    Status read(uint16_t addr, uint8_t *buf, size_t len) override { return impl_.read(addr, buf, len); }
    Status writeRead(uint16_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) override {
        return impl_.writeRead(addr, tx, tx_len, rx, rx_len);
    }

private:
    Impl &impl_;
};

/* ---- ADC virtual interface ----------------------------------------------- */

/** @brief  Continuous ADC conversion callback type.
 *  @param  channel  Channel that produced the sample.
 *  @param  value    Converted value.
 *  @param  ctx      User context pointer. */
using AdcConversionCallback = void (*)(uint8_t channel, uint16_t value, void *ctx);

/** @brief  Virtual ADC interface for dependency injection / GMock. */
class IAdc {
public:
    virtual ~IAdc() = default;
    virtual Status configure(uint8_t channel, uint8_t resolution_bits) = 0;
    virtual uint16_t read(uint8_t channel) = 0;
    virtual Status startContinuous(uint8_t channel, AdcConversionCallback cb, void *ctx) = 0;
    virtual Status stopContinuous(uint8_t channel) = 0;
};

/** @brief  Wrap any CRTP AdcBase implementation as IAdc.
 *  @tparam Impl  Concrete AdcBase implementation. */
template <typename Impl>
class AdcVirtual : public IAdc {
public:
    /** @brief  Construct from a CRTP implementation reference.
     *  @param  impl  Reference to the concrete ADC implementation. */
    explicit AdcVirtual(Impl &impl) : impl_(impl) {}

    Status configure(uint8_t channel, uint8_t resolution_bits) override {
        return impl_.configure(channel, resolution_bits);
    }
    uint16_t read(uint8_t channel) override { return impl_.read(channel); }
    Status startContinuous(uint8_t channel, AdcConversionCallback cb, void *ctx) override {
        return impl_.startContinuous(channel, cb, ctx);
    }
    Status stopContinuous(uint8_t channel) override { return impl_.stopContinuous(channel); }

private:
    Impl &impl_;
};

/* ---- DAC virtual interface ----------------------------------------------- */

/** @brief  Virtual DAC interface for dependency injection / GMock. */
class IDac {
public:
    virtual ~IDac() = default;
    virtual Status configure(uint8_t channel, uint8_t resolution_bits) = 0;
    virtual Status write(uint8_t channel, uint16_t value) = 0;
};

/** @brief  Wrap any CRTP DacBase implementation as IDac.
 *  @tparam Impl  Concrete DacBase implementation. */
template <typename Impl>
class DacVirtual : public IDac {
public:
    /** @brief  Construct from a CRTP implementation reference.
     *  @param  impl  Reference to the concrete DAC implementation. */
    explicit DacVirtual(Impl &impl) : impl_(impl) {}

    Status configure(uint8_t channel, uint8_t resolution_bits) override {
        return impl_.configure(channel, resolution_bits);
    }
    Status write(uint8_t channel, uint16_t value) override { return impl_.write(channel, value); }

private:
    Impl &impl_;
};

/* ---- Timer virtual interface --------------------------------------------- */

/** @brief  Timer callback type.
 *  @param  ctx  User context pointer. */
using TimerCallback = void (*)(void *ctx);

/** @brief  Virtual timer interface for dependency injection / GMock. */
class ITimer {
public:
    virtual ~ITimer() = default;
    virtual Status start(uint32_t period_us) = 0;
    virtual Status stop() = 0;
    virtual Status setCallback(TimerCallback cb, void *ctx) = 0;
    virtual uint32_t getCounterUs() = 0;
};

/** @brief  Wrap any CRTP TimerBase implementation as ITimer.
 *  @tparam Impl  Concrete TimerBase implementation. */
template <typename Impl>
class TimerVirtual : public ITimer {
public:
    /** @brief  Construct from a CRTP implementation reference.
     *  @param  impl  Reference to the concrete timer implementation. */
    explicit TimerVirtual(Impl &impl) : impl_(impl) {}

    Status start(uint32_t period_us) override { return impl_.start(period_us); }
    Status stop() override { return impl_.stop(); }
    Status setCallback(TimerCallback cb, void *ctx) override { return impl_.setCallback(cb, ctx); }
    uint32_t getCounterUs() override { return impl_.getCounterUs(); }

private:
    Impl &impl_;
};

/* ---- CAN virtual interface --------------------------------------------- */

/** @brief  CAN receive callback type.
 *  @param  frame  Received CAN frame.
 *  @param  ctx    User context pointer. */
using CanRxCallback = void (*)(const CanFrame &frame, void *ctx);

/** @brief  Virtual CAN bus interface for dependency injection / GMock. */
class ICan {
public:
    virtual ~ICan() = default;
    virtual Status configure(const CanConfig &cfg) = 0;
    virtual Status send(const CanFrame &frame) = 0;
    virtual Status receive(CanFrame *frame) = 0;
    virtual Status setFilter(uint32_t id, uint32_t mask, bool is_extended) = 0;
    virtual Status setRxCallback(CanRxCallback cb, void *ctx) = 0;
};

/** @brief  Wrap any CRTP CanBase implementation as ICan.
 *  @tparam Impl  Concrete CanBase implementation. */
template <typename Impl>
class CanVirtual : public ICan {
public:
    explicit CanVirtual(Impl &impl) : impl_(impl) {}

    Status configure(const CanConfig &cfg) override { return impl_.configure(cfg); }
    Status send(const CanFrame &frame) override { return impl_.send(frame); }
    Status receive(CanFrame *frame) override { return impl_.receive(frame); }
    Status setFilter(uint32_t id, uint32_t mask, bool is_extended) override {
        return impl_.setFilter(id, mask, is_extended);
    }
    Status setRxCallback(CanRxCallback cb, void *ctx) override { return impl_.setRxCallback(cb, ctx); }

private:
    Impl &impl_;
};

/* ---- PWM virtual interface --------------------------------------------- */

/** @brief  Virtual PWM interface for dependency injection / GMock. */
class IPwm {
public:
    virtual ~IPwm() = default;
    virtual Status start(uint32_t frequency_hz, float duty_percent) = 0;
    virtual Status setDuty(float duty_percent) = 0;
    virtual Status setFrequency(uint32_t frequency_hz) = 0;
    virtual Status stop() = 0;
};

/** @brief  Wrap any CRTP PwmBase implementation as IPwm.
 *  @tparam Impl  Concrete PwmBase implementation. */
template <typename Impl>
class PwmVirtual : public IPwm {
public:
    explicit PwmVirtual(Impl &impl) : impl_(impl) {}

    Status start(uint32_t frequency_hz, float duty_percent) override {
        return impl_.start(frequency_hz, duty_percent);
    }
    Status setDuty(float duty_percent) override { return impl_.setDuty(duty_percent); }
    Status setFrequency(uint32_t frequency_hz) override { return impl_.setFrequency(frequency_hz); }
    Status stop() override { return impl_.stop(); }

private:
    Impl &impl_;
};

/* ---- Power virtual interface ------------------------------------------- */

/** @brief  Virtual power-management interface for dependency injection /
 *          GMock. */
class IPowerManageable {
public:
    virtual ~IPowerManageable() = default;
    virtual Status setPowerState(PowerState state) = 0;
    virtual PowerState getPowerState() = 0;
};

/** @brief  Wrap any CRTP PowerManageable implementation as IPowerManageable.
 *  @tparam Impl  Concrete implementation. */
template <typename Impl>
class PowerManageableVirtual : public IPowerManageable {
public:
    explicit PowerManageableVirtual(Impl &impl) : impl_(impl) {}

    Status setPowerState(PowerState state) override { return impl_.setPowerState(state); }
    PowerState getPowerState() override { return impl_.getPowerState(); }

private:
    Impl &impl_;
};

/* All virtual interfaces complete. Each has a pure virtual class (IXxx)
 * and a template wrapper (XxxVirtual<Impl>) delegating to CRTP impl. */

}  // namespace dp::hal

#endif  // DP_HAL_VIRTUAL_H_
