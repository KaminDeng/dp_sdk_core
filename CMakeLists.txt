# dp_sdk_core/CMakeLists.txt
#
# Unified core abstraction layer for dpsdk.
# Contains three sibling modules:
#   osal   — OS Abstraction Layer (C++17, POSIX / CMSIS-OS2 backends)
#   hal    — Hardware Abstraction Layer (CRTP interfaces, header-only)
#   device — Device management framework (registration, lifecycle)
#
# Consumed by the parent project (dpsdk) via add_subdirectory().
# Not intended for standalone build.

add_subdirectory(osal)
add_subdirectory(hal)
add_subdirectory(device)
