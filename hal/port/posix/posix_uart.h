/** @file   posix_uart.h
 *  @brief  POSIX UART implementation for dp_hal — wraps STDOUT/STDIN. */

#ifndef POSIX_UART_H_
#define POSIX_UART_H_

#include <errno.h>
#include <unistd.h>

#include "dp_uart.h"

namespace dp::hal::posix {

/** @brief  POSIX UART using write(STDOUT_FILENO) / read(STDIN_FILENO). */
class PosixUart : public UartBase<PosixUart> {
    friend class UartBase<PosixUart>;

public:
    static constexpr bool kSupportsDma = false;
    static constexpr size_t kDmaAlignment = 0;

private:
    Status doConfigure(const UartConfig & /*cfg*/) { return Status::kOk; }

    Status doWrite(const uint8_t *buf, size_t len) {
        const uint8_t *p = buf;
        size_t remaining = len;
        while (remaining > 0) {
            ssize_t n = ::write(STDOUT_FILENO, p, remaining);
            if (n < 0) {
                if (errno == EINTR) continue;
                return Status::kError;
            }
            p += n;
            remaining -= static_cast<size_t>(n);
        }
        return Status::kOk;
    }

    Status doRead(uint8_t *buf, size_t max_len, size_t *actual) {
        ssize_t n;
        do {
            n = ::read(STDIN_FILENO, buf, max_len);
        } while (n < 0 && errno == EINTR);
        if (actual != nullptr) {
            *actual = (n > 0) ? static_cast<size_t>(n) : 0;
        }
        return (n >= 0) ? Status::kOk : Status::kError;
    }

    Status doFlush() { return Status::kOk; }

    Status doSetRxCallback(UartBase::RxCallback /*cb*/, void * /*ctx*/) { return Status::kNotSupported; }

    Status doSetTxCompleteCallback(UartBase::TxCompleteCallback /*cb*/, void * /*ctx*/) {
        return Status::kNotSupported;
    }
};

}  // namespace dp::hal::posix

#endif  // POSIX_UART_H_
