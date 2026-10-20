/**
 * @file dp_hal_test_main.cpp
 * @brief HAL GTest 的单翻译单元入口，由父工程测试调度器调用。
 * @details 直接包含各 test_*.cpp，使测试注册与入口留在同一对象文件中，避免静态库链接时
 * 注册对象被裁掉；不要再把这些被包含的测试源单独加入 CMake 源列表。
 * 本文件负责初始化和过滤套件，不负责真实外设接线、硬件烧录或驱动能力鉴定。
 */

#include <gtest/gtest.h>

#include "test_uart.cpp"   // NOLINT(build/include)
#include "test_gpio.cpp"   // NOLINT(build/include)
#include "test_spi.cpp"    // NOLINT(build/include)
#include "test_i2c.cpp"    // NOLINT(build/include)
#include "test_adc.cpp"    // NOLINT(build/include)
#include "test_dac.cpp"    // NOLINT(build/include)
#include "test_timer.cpp"  // NOLINT(build/include)
#include "test_can.cpp"    // NOLINT(build/include)
#include "test_power.cpp"  // NOLINT(build/include)
#include "test_pwm.cpp"    // NOLINT(build/include)
#include "test_virtual.cpp"  // NOLINT(build/include)
#include "test_security.cpp"  // NOLINT(build/include)
#include "test_identity.cpp"  // NOLINT(build/include)

/**
 * @brief 初始化 GoogleTest，限定 HAL 套件过滤器并同步运行所选测试。
 * @details 构造一个本地 argv，不透传进程命令行；过滤器覆盖外设、虚接口、安全与身份套件。
 * RUN_ALL_TESTS 的结果保存后被显式丢弃，本入口返回 void，调用方需检查测试报告而不能
 * 仅凭此函数返回或宿主退出码宣称所有断言成功。
 * @note 运行上下文与 mock/产品端口由父工程装配；本入口不切换后端。
 */
extern "C" void dp_hal_test_main(void) {
    // argv0 必须是可写数组并在初始化期间存活；没有额外的命令行参数。
    int argc = 1;
    char argv0[] = "dp_hal_tests";
    char* argv[] = {argv0, nullptr};
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(filter) =
        "DpHalUart*:DpHalGpio*:DpHalSpiBus*:DpHalSpiDevice*:"
        "DpHalI2cBus*:DpHalI2cDevice*:"
        "DpHalAdc*:DpHalDac*:DpHalTimer*:DpHalCan*:DpHalPower*:DpHalPwm*:DpHalVirtual*:DpHalSecurity*:"
        "DpHalIdentity*";
    int r = RUN_ALL_TESTS();
    (void)r;
}
