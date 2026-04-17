/** @file   dp_hal_port_impl.cpp
 *  @brief  Port template for dp_hal -- copy this file to your port directory
 *          and implement each function.
 *
 *  Every function has an #error sentinel that must be removed once implemented.
 */

#include "dp_hal_port.h"

#include <stddef.h>
#include <stdint.h>

/* ---- dp_hal_port_time_us ------------------------------------------------ */
#error "Implement dp_hal_port_time_us() for your platform"
uint64_t dp_hal_port_time_us(void) { return 0; }

/* ---- dp_hal_port_log ---------------------------------------------------- */
#error "Implement dp_hal_port_log() for your platform"
void dp_hal_port_log(const char *buf, size_t len) {
    (void)buf;
    (void)len;
}

/* ---- dp_hal_port_assert_fail -------------------------------------------- */
#error "Implement dp_hal_port_assert_fail() for your platform"
void dp_hal_port_assert_fail(const char *expr, const char *file, int line) {
    (void)expr;
    (void)file;
    (void)line;
    while (1) {
    }
}
