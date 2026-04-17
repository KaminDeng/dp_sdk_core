# kernel/dp_hal/test/CMakeLists.txt
#
# dp_hal test static library (linked into firmware test binary).
# Test .cpp files are #included by dp_hal_test_main.cpp (single TU pattern).

add_library(dp_hal_test STATIC
    dp_hal_test_main.cpp
)

target_include_directories(dp_hal_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(dp_hal_test
    PUBLIC dp_hal
           gtest
)
