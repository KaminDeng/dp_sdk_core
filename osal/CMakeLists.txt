cmake_minimum_required(VERSION 3.15)

dpsdk_library(
    NAME          osal
    DESCRIPTION   "C++17 OS 抽象层"
    NOTES         "通过 osal_port 选择后端（POSIX/CMSIS-OS2）"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── OSAL 原语裁剪 ──────────────────────────────────────
option(OSAL_ENABLE_THREAD_POOL    "线程池"         ON)
option(OSAL_ENABLE_RW_LOCK        "读写锁"         ON)
option(OSAL_ENABLE_SPIN_LOCK      "自旋锁"         ON)
option(OSAL_ENABLE_CONDITION_VAR  "条件变量"        ON)
option(OSAL_ENABLE_TIMER          "软件定时器"      ON)
option(OSAL_ENABLE_CHRONO         "高精度时钟"      ON)
option(OSAL_ENABLE_MEMORY_MANAGER "OSAL 内存管理"   ON)
option(OSAL_STATIC_ALLOC          "静态分配模式"    OFF)

# ── Port directory resolution ──────────────────────────────────────────────────
# Users set OSAL_PORT_DIR before add_subdirectory(osal) to supply their port:
#   set(OSAL_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/my_port)
#
# If OSAL_PORT_DIR is not set, the built-in POSIX port is used (stdout debug,
# all tests enabled). This is suitable for Linux/macOS development out of the box.
if (NOT DEFINED OSAL_PORT_DIR)
    message(STATUS "[osal] OSAL_PORT_DIR not set — using built-in POSIX port (port/builtin_posix)")
    set(OSAL_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port/builtin_posix")
endif()

if (NOT EXISTS "${OSAL_PORT_DIR}/osal_port.h")
    message(FATAL_ERROR
        "[osal] osal_port.h not found in OSAL_PORT_DIR='${OSAL_PORT_DIR}'.\n"
        "Copy osal/port/template/osal_port.h to '${OSAL_PORT_DIR}/' and fill it in.\n"
        "Then set OSAL_PORT_DIR to that directory before add_subdirectory(osal).")
endif()

# ── Backend auto-detection ─────────────────────────────────────────────────────
# Scan osal_port.h for the OSAL_BACKEND_CMSIS_OS macro (non-commented line).
# Defaults to posix if not found. Defining both backends is undefined behaviour.
file(STRINGS "${OSAL_PORT_DIR}/osal_port.h" _osal_port_lines)
set(_osal_backend "posix")
foreach(_line IN LISTS _osal_port_lines)
    if (_line MATCHES "^[ \t]*#[ \t]*define[ \t]+OSAL_BACKEND_CMSIS_OS")
        set(_osal_backend "cmsis_os")
        break()
    endif()
endforeach()
message(STATUS "[osal] Port directory: ${OSAL_PORT_DIR}")
message(STATUS "[osal] Backend:        ${_osal_backend}")

# ── Source selection ───────────────────────────────────────────────────────────
set(OSAL_IMPL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/impl")
set(GOOGLETEST_SUITE_PATH "test/gtest")

file(GLOB_RECURSE SYSTEM_IMPL_SOURCES "${OSAL_IMPL_PATH}/${_osal_backend}/*.cpp")
file(GLOB_RECURSE GTEST_SUITE_SOURCES "${GOOGLETEST_SUITE_PATH}/*.cpp")

message(STATUS "[osal] Impl sources:   ${SYSTEM_IMPL_SOURCES}")

# ── Library target ─────────────────────────────────────────────────────────────
add_library(osal STATIC
    src/debug/osal_debug.cpp
    ${SYSTEM_IMPL_SOURCES}
    test/osal_test_main.cpp
    ${GTEST_SUITE_SOURCES}
)

# ── Include directories ────────────────────────────────────────────────────────
# OSAL_PORT_DIR is PUBLIC so downstream targets can also see osal_port.h
# (needed if they use osal types directly or include any osal header).
target_include_directories(osal
    PUBLIC
        "${OSAL_PORT_DIR}"
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
#   target_link_libraries(osal PUBLIC freertos_kernel cmsis_os2_impl)
target_link_libraries(osal PUBLIC gtest)

# osal_debug.h uses ##__VA_ARGS__ (GNU extension) to support zero-arg variadic
# macro calls. Suppress the pedantic warning about this GNU extension.
target_compile_options(osal PUBLIC -Wno-gnu-zero-variadic-macro-arguments)

# ── 原语裁剪：传播编译定义 ────────────────────────────────────────────────────
foreach(_feat THREAD_POOL RW_LOCK SPIN_LOCK CONDITION_VAR TIMER CHRONO MEMORY_MANAGER)
    if(OSAL_ENABLE_${_feat})
        target_compile_definitions(osal PUBLIC OSAL_ENABLE_${_feat}=1)
    else()
        target_compile_definitions(osal PUBLIC OSAL_ENABLE_${_feat}=0)
    endif()
endforeach()
# STATIC_ALLOC 使用不同的 option 名
if(OSAL_STATIC_ALLOC)
    target_compile_definitions(osal PUBLIC OSAL_ENABLE_STATIC_ALLOC=1)
else()
    target_compile_definitions(osal PUBLIC OSAL_ENABLE_STATIC_ALLOC=0)
endif()
