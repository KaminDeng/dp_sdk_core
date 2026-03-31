/** @file   dp_spi.h
 *  @brief  SPI bus controller and SPI device CRTP interfaces for dp_hal.
 *
 *  SpiBusBase Impl must provide:
 *    - static constexpr bool kSupportsDma
 *    - static constexpr size_t kDmaAlignment  (0 = no requirement)
 *    - Status doConfigure(const SpiConfig& cfg)
 *    - Status doTransfer(const uint8_t* tx, uint8_t* rx, size_t len)
 *    - Status doWrite(const uint8_t* buf, size_t len)
 *    - Status doRead(uint8_t* buf, size_t len)
 *    - Status doTransferAsync(const uint8_t* tx, uint8_t* rx, size_t len,
 *                              TransferCompleteCallback cb, void* ctx) */

#ifndef DP_SPI_H_
#define DP_SPI_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  SPI bus controller interface (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class SpiBusBase {
public:
    /** @brief  Whether the implementation supports DMA transfers. */
    static constexpr bool kSupportsDma = Impl::kSupportsDma;

    /** @brief  Required DMA buffer alignment (0 = no requirement). */
    static constexpr size_t kDmaAlignment = Impl::kDmaAlignment;

    /** @brief  Configure the SPI bus.
     *  @param  cfg  SPI configuration parameters.
     *  @return Status::kOk on success. */
    Status configure(const SpiConfig &cfg) { return impl().doConfigure(cfg); }

    /** @brief  Full-duplex transfer.
     *  @param  tx   Transmit buffer.
     *  @param  rx   Receive buffer.
     *  @param  len  Number of bytes.
     *  @return Status::kOk on success. */
    Status transfer(const uint8_t *tx, uint8_t *rx, size_t len) { return impl().doTransfer(tx, rx, len); }

    /** @brief  Write-only transfer.
     *  @param  buf  Transmit buffer.
     *  @param  len  Number of bytes.
     *  @return Status::kOk on success. */
    Status write(const uint8_t *buf, size_t len) { return impl().doWrite(buf, len); }

    /** @brief  Read-only transfer.
     *  @param  buf  Receive buffer.
     *  @param  len  Number of bytes.
     *  @return Status::kOk on success. */
    Status read(uint8_t *buf, size_t len) { return impl().doRead(buf, len); }

    /** @brief  Asynchronous transfer complete callback type.
     *  @param  ctx  User context pointer. */
    using TransferCompleteCallback = void (*)(void *ctx);

    /** @brief  Initiate an asynchronous full-duplex transfer.
     *  @param  tx   Transmit buffer (must remain valid until callback).
     *  @param  rx   Receive buffer (must remain valid until callback).
     *  @param  len  Number of bytes.
     *  @param  cb   Completion callback.
     *  @param  ctx  User context pointer.
     *  @return Status::kOk on success, kNotSupported if async not available. */
    Status transferAsync(const uint8_t *tx, uint8_t *rx, size_t len, TransferCompleteCallback cb, void *ctx) {
        return impl().doTransferAsync(tx, rx, len, cb, ctx);
    }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

/** @brief  SPI slave device = bus + CS pin (platform-independent framework).
 *  @tparam SpiBus  SPI bus controller type (must satisfy SpiBusBase interface).
 *  @tparam CsPin   GPIO pin type (must satisfy GpioPinBase interface). */
template <typename SpiBus, typename CsPin>
class SpiDevice {
public:
    /** @brief  Construct an SPI device.
     *  @param  bus  Reference to the SPI bus controller.
     *  @param  cs   Reference to the chip-select GPIO pin. */
    SpiDevice(SpiBus &bus, CsPin &cs) : bus_(bus), cs_(cs) {}

    /** @brief  Full-duplex transfer with automatic CS toggle.
     *  @param  tx   Transmit buffer.
     *  @param  rx   Receive buffer.
     *  @param  len  Number of bytes.
     *  @return Status::kOk on success. */
    Status transfer(const uint8_t *tx, uint8_t *rx, size_t len) {
        cs_.write(PinState::kActive);
        Status s = bus_.transfer(tx, rx, len);
        cs_.write(PinState::kInactive);
        return s;
    }

    /** @brief  Write-only transfer with automatic CS toggle.
     *  @param  buf  Transmit buffer.
     *  @param  len  Number of bytes.
     *  @return Status::kOk on success. */
    Status write(const uint8_t *buf, size_t len) {
        cs_.write(PinState::kActive);
        Status s = bus_.write(buf, len);
        cs_.write(PinState::kInactive);
        return s;
    }

    /** @brief  Read-only transfer with automatic CS toggle.
     *  @param  buf  Receive buffer.
     *  @param  len  Number of bytes.
     *  @return Status::kOk on success. */
    Status read(uint8_t *buf, size_t len) {
        cs_.write(PinState::kActive);
        Status s = bus_.read(buf, len);
        cs_.write(PinState::kInactive);
        return s;
    }

private:
    SpiBus &bus_;
    CsPin &cs_;
};

}  // namespace dp::hal

#endif  // DP_SPI_H_
