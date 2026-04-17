# dp_sdk_core/CMakeLists.txt
#
# Unified core abstraction layer for dpsdk.
# Contains three sibling modules:
#   dp_osal   — OS Abstraction Layer (C++17, POSIX / CMSIS-OS2 backends)
#   dp_hal    — Hardware Abstraction Layer (CRTP interfaces, header-only)
#   dp_device — Device management framework (registration, lifecycle)
#
# Consumed by the parent project (dpsdk) via add_subdirectory().
# Not intended for standalone build.

# dpsdk_compat：跨平台兼容宏（DPSDK_USED、DPSDK_WEAK 等）
add_library(dpsdk_compat INTERFACE)
target_include_directories(dpsdk_compat INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

add_subdirectory(dp_osal)
add_subdirectory(dp_hal)
add_subdirectory(dp_device)

