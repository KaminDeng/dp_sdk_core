#include <gtest/gtest.h>

#include "dp_hal_port.h"

TEST(DpHalIdentity, ReturnsStableNonzeroDeviceId) {
    dp_hal_device_id_t first{};
    dp_hal_device_id_t second{};
    ASSERT_EQ(dp_hal_port_get_device_id(&first), DP_HAL_DEVICE_ID_OK);
    ASSERT_EQ(dp_hal_port_get_device_id(&second), DP_HAL_DEVICE_ID_OK);
    bool nonzero = false;
    for (size_t index = 0U; index < sizeof(first.bytes); ++index) {
        nonzero = nonzero || first.bytes[index] != 0U;
        EXPECT_EQ(first.bytes[index], second.bytes[index]);
    }
    EXPECT_TRUE(nonzero);
    EXPECT_EQ(dp_hal_port_get_device_id(nullptr), DP_HAL_DEVICE_ID_INVALID_ARGUMENT);
}
