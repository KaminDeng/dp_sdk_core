/** @file dp_hal_can_fd_test_main.cpp
 * @brief Core自身的CAN FD合同测试入口，不依赖父仓通信库或产品任务。
 * @details 单TU保留GTest注册，真实失败经进程退出码反馈。只验证Host合同，不授物理FD资格。
 */
#include <gtest/gtest.h>
#include "test_can_fd_contract.cpp"
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
