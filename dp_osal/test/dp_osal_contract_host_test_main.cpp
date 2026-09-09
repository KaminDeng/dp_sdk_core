#include <gtest/gtest.h>
#include <cstring>
#include "gtest_contract_compat.cpp"
int main(int argc, char** argv) {
#if DP_OSAL_ENABLE_CHRONO
    if (argc == 2 && std::strcmp(argv[1], "--dp-osal-unsupported-chrono") == 0)
        return runUnsupportedChronoDefault();
#endif
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    return RUN_ALL_TESTS();
}
