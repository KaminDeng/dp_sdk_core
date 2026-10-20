/**
 * @file dp_osal_chrono_host_test_main.cpp
 * @brief 通用时钟期限的独立主机GTest入口。
 * @details 以直接包含测试源的单翻译单元方式保留注册；时钟套件受CHRONO开关控制。
 * 与固件void入口不同，本main把RUN_ALL_TESTS结果作为进程状态返回。
 * 这里没有设备访问或硬件时序资格认证；是否启用各具体测试还要查看套件自身宏。
 */
#include <gtest/gtest.h>

#if DP_OSAL_ENABLE_CHRONO
#include "gtest_chrono_deadline.cpp"
#endif

/**
 * @brief 解析GoogleTest命令行并运行本翻译单元注册的套件。
 * @param argc 进程参数个数，InitGoogleTest可消费已识别选项。
 * @param argv 进程参数数组。
 * @return RUN_ALL_TESTS的状态，供主机测试调度观察。
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
