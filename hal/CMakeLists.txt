# kernel/hal/CMakeLists.txt
#
# dp_hal -- device abstraction layer (CRTP interfaces, zero platform flags).
#
# Produces:
#   dp_hal            -- INTERFACE: CRTP headers + port headers
#   dp_hal_port_impl  -- STATIC (via port subdirectory): port functions
#   dp_hal_test       -- STATIC (optional): GTest suites

cmake_minimum_required(VERSION 3.15)

dpsdk_library(
    NAME          dp_hal
    DESCRIPTION   "零开销 CRTP 硬件抽象层"
    PRIMARY_TAG   general
    TAGS          general hal
    NOTES         "INTERFACE 库（仅头文件），通过 dp_hal_port_impl 提供实现"
)

# ── INTERFACE library (header-only CRTP interfaces) ───────────────────────
add_library(dp_hal INTERFACE)

target_include_directories(dp_hal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── Port directory resolution ─────────────────────────────────────────────
# HAL_PORT_DIR is set by resolve_platform_ports.cmake or explicitly by the
# product cmake. Fallback order:
#   1. platform/<KERNEL_PORT>/hal_port/    (product-specific)
#   2. platform/common/hal_port/           (shared default)
#   3. kernel/hal/port/mock/               (built-in mock for host testing)
if(NOT DEFINED HAL_PORT_DIR)
    set(_product_hal "${CMAKE_SOURCE_DIR}/platform/${KERNEL_PORT}/hal_port")
    set(_common_hal  "${CMAKE_SOURCE_DIR}/platform/common/hal_port")
    set(_mock_hal    "${CMAKE_CURRENT_SOURCE_DIR}/port/mock")

    if(IS_DIRECTORY "${_product_hal}")
        set(HAL_PORT_DIR "${_product_hal}")
        message(STATUS "[dp_hal] Port directory: ${HAL_PORT_DIR}")
    elseif(IS_DIRECTORY "${_common_hal}")
        set(HAL_PORT_DIR "${_common_hal}")
        message(STATUS "[dp_hal] Port directory: ${HAL_PORT_DIR} (common)")
    elseif(IS_DIRECTORY "${_mock_hal}")
        set(HAL_PORT_DIR "${_mock_hal}")
        message(STATUS "[dp_hal] Port directory: ${HAL_PORT_DIR} (mock)")
    else()
        message(FATAL_ERROR "[dp_hal] No port directory found")
    endif()
endif()

# Include port headers in dp_hal INTERFACE
target_include_directories(dp_hal INTERFACE ${HAL_PORT_DIR})

# Build the port implementation
add_subdirectory(${HAL_PORT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/hal_port)
target_link_libraries(dp_hal INTERFACE dp_hal_port_impl)

# ── Tests (optional) ──────────────────────────────────────────────────────
option(DP_HAL_BUILD_TESTS "Build dp_hal GTest suite" OFF)

if(DP_HAL_BUILD_TESTS)
    add_subdirectory(test)
endif()
