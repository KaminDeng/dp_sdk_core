/**
 * @file test_identity.cpp
 * @brief 检查当前 HAL 端口身份读取的重复稳定性、整体非零及空参数返回值。
 * @details 由 dp_hal_test_main.cpp 直接包含；mock 上只覆盖固定测试身份，
 * 不能据此证明两块真实设备身份不同，也未覆盖跨重启持久性或不可伪造性。
 */
#include <gtest/gtest.h>

#include "dp_hal_port.h"

/**
 * @brief 连读两次逐字节比较，确认至少一字节非零，并检查空输出指针被拒绝。
 * @details ASSERT_EQ 在读取失败时终止本测试，避免把无效输出当作已获得的身份。
 * 非零判断是整个 128 位值非零，不要求每个字节均非零。
 */
TEST(DpHalIdentity, ReturnsStableNonzeroDeviceId) {
    dp_hal_device_id_t first{};
    dp_hal_device_id_t second{};
    ASSERT_EQ(dp_hal_port_get_device_id(&first), DP_HAL_DEVICE_ID_OK);
    ASSERT_EQ(dp_hal_port_get_device_id(&second), DP_HAL_DEVICE_ID_OK);
    bool nonzero = false;  // 遍历时累计“发现过非零字节”，同时比较两次读取。
    for (size_t index = 0U; index < sizeof(first.bytes); ++index) {
        nonzero = nonzero || first.bytes[index] != 0U;
        EXPECT_EQ(first.bytes[index], second.bytes[index]);
    }
    EXPECT_TRUE(nonzero);
    EXPECT_EQ(dp_hal_port_get_device_id(nullptr), DP_HAL_DEVICE_ID_INVALID_ARGUMENT);
}
