# kernel/device/CMakeLists.txt
#
# dp_device — device management framework on top of dp_hal.
# Provides Device base class, DeviceManager registry, and per-class adapters.

add_library(dp_device STATIC
    src/dp_device.cpp
    src/dp_device_manager.cpp
)

target_include_directories(dp_device PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dp_device PUBLIC dp_hal PRIVATE osal)

option(DP_DEVICE_BUILD_TESTS "Build dp_device GTest suite" OFF)
if(DP_DEVICE_BUILD_TESTS)
    add_subdirectory(test)
endif()
