/** @file   dp_hal_test_main.cpp
 *  @brief  GTest entry point for dp_hal test suites.
 *
 *  All test .cpp files are #included here (single TU pattern) to prevent
 *  linker dead-stripping of GTest registrations from static archives. */

#include <gtest/gtest.h>

#include "test_uart.cpp"   // NOLINT(build/include)
#include "test_gpio.cpp"   // NOLINT(build/include)
#include "test_spi.cpp"    // NOLINT(build/include)
#include "test_i2c.cpp"    // NOLINT(build/include)
#include "test_adc.cpp"    // NOLINT(build/include)
#include "test_dac.cpp"    // NOLINT(build/include)
#include "test_timer.cpp"  // NOLINT(build/include)
#include "test_power.cpp"  // NOLINT(build/include)

/** @brief  dp_hal test entry point.
 *
 *  Called from the firmware's main test dispatcher. Sets the GTest filter
 *  to restrict execution to dp_hal suites only. */
extern "C" void dp_hal_test_main(void) {
    int argc = 1;
    char argv0[] = "dp_hal_tests";
    char* argv[] = {argv0, nullptr};
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(filter) =
        "DpHalUart*:DpHalGpio*:DpHalSpiBus*:DpHalSpiDevice*:"
        "DpHalI2cBus*:DpHalI2cDevice*:"
        "DpHalAdc*:DpHalDac*:DpHalTimer*:DpHalPower*";
    int r = RUN_ALL_TESTS();
    (void)r;
}
