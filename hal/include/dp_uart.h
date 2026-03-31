/** @file   dp_uart.h
 *  @brief  UART device CRTP interface for dp_hal.
 *
 *  Impl must provide:
 *    - static constexpr bool kSupportsDma
 *    - static constexpr size_t kDmaAlignment  (0 = no requirement)
 *    - Status doConfigure(const UartConfig& cfg)
 *    - Status doWrite(const uint8_t* buf, size_t len)
 *    - Status doRead(uint8_t* buf, size_t max_len, size_t* actual)
 *    - Status doFlush()
 *    - Status doSetRxCallback(RxCallback cb, void* ctx)
 *    - Status doSetTxCompleteCallback(TxCompleteCallback cb, void* ctx) */

#ifndef DP_UART_H_
#define DP_UART_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_types.h"

namespace dp::hal {

/** @brief  UART device interface (CRTP).
 *  @tparam Impl  Concrete implementation class. */
template <typename Impl>
class UartBase {
public:
    /** @brief  Whether the implementation supports DMA transfers. */
    static constexpr bool kSupportsDma = Impl::kSupportsDma;

    /** @brief  Required DMA buffer alignment (0 = no requirement). */
    static constexpr size_t kDmaAlignment = Impl::kDmaAlignment;

    /** @brief  Configure the UART peripheral.
     *  @param  cfg  UART configuration parameters.
     *  @return Status::kOk on success. */
    Status configure(const UartConfig &cfg) { return impl().doConfigure(cfg); }

    /** @brief  Write data to the UART (blocking).
     *  @param  buf  Data buffer to transmit.
     *  @param  len  Number of bytes to transmit.
     *  @return Status::kOk on success. */
    Status write(const uint8_t *buf, size_t len) { return impl().doWrite(buf, len); }

    /** @brief  Read data from the UART.
     *  @param  buf      Receive buffer.
     *  @param  max_len  Maximum bytes to read.
     *  @param  actual   Pointer to store actual bytes read.
     *  @return Status::kOk on success. */
    Status read(uint8_t *buf, size_t max_len, size_t *actual) { return impl().doRead(buf, max_len, actual); }

    /** @brief  Flush pending transmit data.
     *  @return Status::kOk on success. */
    Status flush() { return impl().doFlush(); }

    /** @brief  Receive data callback type.
     *  @param  data  Pointer to received data.
     *  @param  len   Number of bytes received.
     *  @param  ctx   User context pointer. */
    using RxCallback = void (*)(const uint8_t *data, size_t len, void *ctx);

    /** @brief  Transmit-complete callback type.
     *  @param  ctx  User context pointer. */
    using TxCompleteCallback = void (*)(void *ctx);

    /** @brief  Register a receive data callback.
     *  @param  cb   Callback function (nullptr to unregister).
     *  @param  ctx  User context pointer.
     *  @return Status::kOk on success, kNotSupported if async not available. */
    Status setRxCallback(RxCallback cb, void *ctx) { return impl().doSetRxCallback(cb, ctx); }

    /** @brief  Register a transmit-complete callback.
     *  @param  cb   Callback function (nullptr to unregister).
     *  @param  ctx  User context pointer.
     *  @return Status::kOk on success, kNotSupported if async not available. */
    Status setTxCompleteCallback(TxCompleteCallback cb, void *ctx) { return impl().doSetTxCompleteCallback(cb, ctx); }

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace dp::hal

#endif  // DP_UART_H_
