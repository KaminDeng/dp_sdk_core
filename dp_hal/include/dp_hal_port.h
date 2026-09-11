/** @file   dp_hal_port.h
 *  @brief  Port interface declarations for dp_hal (pure interface, no impl).
 *
 *  Each port backend must implement these functions in dp_hal_port_impl.cpp. */

#ifndef DP_HAL_PORT_H_
#define DP_HAL_PORT_H_

#include <stddef.h>
#include <stdint.h>

#include "dp_hal_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t bytes[16];
} dp_hal_device_id_t;

typedef enum {
    DP_HAL_DEVICE_ID_OK = 0,
    DP_HAL_DEVICE_ID_INVALID_ARGUMENT = -1,
    DP_HAL_DEVICE_ID_UNAVAILABLE = -2,
} dp_hal_device_id_status_t;

/** @brief Read the stable, nonzero 128-bit deployment device identity. */
dp_hal_device_id_status_t dp_hal_port_get_device_id(dp_hal_device_id_t *out);

/** @brief  Return hardware monotonic tick in microseconds.
 *  @note   Must be safe to call from ISR context (no OSAL dependency). */
uint64_t dp_hal_port_time_us(void);

/** @brief  Low-level debug output (pre-OSAL, ISR-safe).
 *  @param  buf  Data to write.
 *  @param  len  Number of bytes. */
void dp_hal_port_log(const char *buf, size_t len);

/** @brief  Assertion failure handler. Must not return.
 *  @param  expr  Stringified assertion expression.
 *  @param  file  Source file name.
 *  @param  line  Source line number. */
DP_HAL_NORETURN void dp_hal_port_assert_fail(const char *expr, const char *file, int line);

#ifdef __cplusplus
}
#endif

/** @brief  Runtime assertion macro for dp_hal. */
#define DP_HAL_ASSERT(cond)                                            \
    do {                                                               \
        if (!(cond)) {                                                 \
            dp_hal_port_assert_fail(#cond, DP_HAL_FILENAME, __LINE__); \
        }                                                              \
    } while (0)

#endif  // DP_HAL_PORT_H_
