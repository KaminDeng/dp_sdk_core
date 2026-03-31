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

/* All virtual interfaces complete. Each has a pure virtual class (IXxx)
 * and a template wrapper (XxxVirtual<Impl>) delegating to CRTP impl. */

}  // namespace dp::hal

#endif  // DP_HAL_VIRTUAL_H_
