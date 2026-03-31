/** @file   dp_device_test_main.cpp
 *  @brief  GTest entry point for dp_device test suites.
 *
 *  All test .cpp files are #included here (single TU pattern) to prevent
 *  linker dead-stripping of GTest registrations from static archives. */

#include <gtest/gtest.h>

#include "test_device.cpp"          // NOLINT(build/include)
#include "test_device_manager.cpp"  // NOLINT(build/include)
#include "test_serial_device.cpp"   // NOLINT(build/include)
#include "test_pin_device.cpp"      // NOLINT(build/include)

/** @brief  dp_device test entry point.
 *
 *  Called from the firmware's main test dispatcher. Sets the GTest filter
 *  to restrict execution to dp_device suites only. */
extern "C" void dp_device_test_main(void) {
    int argc = 1;
    char argv0[] = "dp_device_tests";
    char* argv[] = {argv0, nullptr};
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(filter) = "DpDevice*:DpDeviceManager*:DpSerialDevice*:DpPinDevice*";
    int r = RUN_ALL_TESTS();
    (void)r;
}
