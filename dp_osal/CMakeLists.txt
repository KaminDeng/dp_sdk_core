cmake_minimum_required(VERSION 3.15)

dpsdk_library(
    NAME          dp_osal
    DESCRIPTION   "C++17 OS 抽象层"
    PRIMARY_TAG   general
    TAGS          general dp_osal
    NOTES         "通过 dp_osal_port 选择后端（POSIX/CMSIS-OS2）"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── OSAL 原语裁剪 ──────────────────────────────────────
option(DP_OSAL_ENABLE_THREAD_POOL    "线程池"         ON)
option(DP_OSAL_ENABLE_RW_LOCK        "读写锁"         ON)
option(DP_OSAL_ENABLE_SPIN_LOCK      "自旋锁"         ON)
option(DP_OSAL_ENABLE_CONDITION_VAR  "条件变量"        ON)
option(DP_OSAL_ENABLE_TIMER          "软件定时器"      ON)
option(DP_OSAL_ENABLE_CHRONO         "高精度时钟"      ON)
option(DP_OSAL_ENABLE_MEMORY_MANAGER "OSAL 内存管理"   ON)
option(DP_OSAL_ENABLE_THREAD_SNAPSHOT  "线程快照（top）"  ON)
option(DP_OSAL_STATIC_ALLOC          "静态分配模式"    OFF)

# ── Port directory resolution ──────────────────────────────────────────────────
# Users set DP_OSAL_PORT_DIR before add_subdirectory(dp_osal) to supply their port:
#   set(DP_OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)
#
# If DP_OSAL_PORT_DIR is not set, the built-in POSIX port is used (stdout debug,
# all tests enabled). This is suitable for Linux/macOS development out of the box.
if (NOT DEFINED DP_OSAL_PORT_DIR)
    message(STATUS "[dp_osal] DP_OSAL_PORT_DIR not set — using built-in POSIX port (port/builtin_posix)")
    set(DP_OSAL_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port/builtin_posix")
endif()

if (NOT EXISTS "${DP_OSAL_PORT_DIR}/dp_osal_port.h")
    message(FATAL_ERROR
        "[dp_osal] dp_osal_port.h not found in DP_OSAL_PORT_DIR='${DP_OSAL_PORT_DIR}'.\n"
        "Copy dp_osal/port/template/dp_osal_port.h to '${DP_OSAL_PORT_DIR}/' and fill it in.\n"
        "Then set DP_OSAL_PORT_DIR to that directory before add_subdirectory(dp_osal).")
endif()

# ── Backend auto-detection ─────────────────────────────────────────────────────
# Scan dp_osal_port.h for the DP_OSAL_BACKEND_CMSIS_OS macro (non-commented line).
# Defaults to posix if not found. Defining both backends is undefined behaviour.
file(STRINGS "${DP_OSAL_PORT_DIR}/dp_osal_port.h" _osal_port_lines)
set(_osal_backend "posix")
foreach(_line IN LISTS _osal_port_lines)
    if (_line MATCHES "^[ \t]*#[ \t]*define[ \t]+DP_OSAL_BACKEND_CMSIS_OS")
        set(_osal_backend "cmsis_os")
        break()
    endif()
endforeach()
message(STATUS "[dp_osal] Port directory: ${DP_OSAL_PORT_DIR}")
message(STATUS "[dp_osal] Backend:        ${_osal_backend}")

# ── Source selection ───────────────────────────────────────────────────────────
set(OSAL_IMPL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/impl")
set(GOOGLETEST_SUITE_PATH "test/gtest")

file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${OSAL_IMPL_PATH}/${_osal_backend}/*.cpp")
file(GLOB_RECURSE GTEST_SUITE_SOURCES "${GOOGLETEST_SUITE_PATH}/*.cpp")

message(STATUS "[dp_osal] Impl sources:   ${SYSTEM_IMPL_SOURCES}")

# ── Library target ─────────────────────────────────────────────────────────────
add_library(dp_osal STATIC
    src/debug/dp_osal_debug.cpp
    ${SYSTEM_IMPL_SOURCES}
    test/dp_osal_test_main.cpp
    ${GTEST_SUITE_SOURCES}
)

# ── Include directories ────────────────────────────────────────────────────────
# DP_OSAL_PORT_DIR is PUBLIC so downstream targets can also see dp_osal_port.h
# (needed if they use dp_osal types directly or include any dp_osal header).
target_include_directories(dp_osal
    PUBLIC
        "${DP_OSAL_PORT_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/debug"
        $<$<STREQUAL:${_osal_backend},cmsis_os>:${CMAKE_CURRENT_SOURCE_DIR}/src/impl/cmsis_os/include>
        $<$<STREQUAL:${_osal_backend},posix>:${CMAKE_CURRENT_SOURCE_DIR}/src/impl/posix/include>
        "${CMAKE_CURRENT_SOURCE_DIR}/test"
        "${CMAKE_CURRENT_SOURCE_DIR}/${GOOGLETEST_SUITE_PATH}"
)

# ── Dependencies ───────────────────────────────────────────────────────────────
# gtest is always linked (test sources always compiled).
# For CMSIS-OS ports that depend on additional platform libraries, the user
# must link them externally:
#   target_link_libraries(dp_osal PUBLIC freertos_kernel cmsis_os2_impl)
target_link_libraries(dp_osal PUBLIC gtest)

# osal_debug.h uses ##__VA_ARGS__ (GNU extension) to support zero-arg variadic
# macro calls. Suppress the pedantic warning about this GNU extension.
target_compile_options(dp_osal PUBLIC -Wno-gnu-zero-variadic-macro-arguments)

# ── 原语裁剪：传播编译定义 ────────────────────────────────────────────────────
foreach(_feat THREAD_POOL RW_LOCK SPIN_LOCK CONDITION_VAR TIMER CHRONO MEMORY_MANAGER THREAD_SNAPSHOT)
    if(DP_OSAL_ENABLE_${_feat})
        target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_${_feat}=1)
    else()
        target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_${_feat}=0)
    endif()
endforeach()
# STATIC_ALLOC 使用不同的 option 名
if(DP_OSAL_STATIC_ALLOC)
    target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_STATIC_ALLOC=1)
else()
    target_compile_definitions(dp_osal PUBLIC DP_OSAL_ENABLE_STATIC_ALLOC=0)
endif()
