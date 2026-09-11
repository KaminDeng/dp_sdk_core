/** @file   dp_hal_port_impl.cpp
 *  @brief  Mock port implementation for dp_hal (host testing).
 *
 *  Uses std::chrono for time, dp_log for log (with dp_console fallback),
 *  abort() for assert. */

#include "dp_hal_port.h"

#include <chrono>  // NOLINT(build/c++11)
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "dp_console.h"
#include "dp_log.h"

dp_hal_device_id_status_t dp_hal_port_get_device_id(dp_hal_device_id_t *out) {
    if (out == nullptr) return DP_HAL_DEVICE_ID_INVALID_ARGUMENT;
    static constexpr uint8_t kMockId[16] = {
        0x4DU, 0x4FU, 0x43U, 0x4BU, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U,
    };
    std::memcpy(out->bytes, kMockId, sizeof(kMockId));
    return DP_HAL_DEVICE_ID_OK;
}

/* ---- dp_hal_port_time_us ------------------------------------------------ */
uint64_t dp_hal_port_time_us(void) {
    using namespace std::chrono;  // NOLINT(build/namespaces)
    auto now = steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(duration_cast<microseconds>(now).count());
}

/* ---- dp_hal_port_log ---------------------------------------------------- */
static dp_log_tag_t s_tag_hal = DP_LOG_INVALID_TAG;

void dp_hal_port_log(const char *buf, size_t len) {
    if (dp_log_is_initialized()) {
        if (s_tag_hal == DP_LOG_INVALID_TAG) {
            s_tag_hal = dp_log_tag_register("HAL");
        }
        dp_log_write(DP_LOG_DEBUG, s_tag_hal, NULL, 0, "%.*s", static_cast<int>(len), buf);
    } else {
        dp_console_write(buf, len);
    }
}

/* ---- dp_hal_port_assert_fail -------------------------------------------- */
void dp_hal_port_assert_fail(const char *expr, const char *file, int line) {
    std::fprintf(stderr, "DP_HAL_ASSERT failed: %s [%s:%d]\n", expr, file, line);
    std::abort();
}
