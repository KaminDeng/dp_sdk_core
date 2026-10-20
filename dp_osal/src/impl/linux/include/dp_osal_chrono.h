/**
 * @file dp_osal_chrono.h
 * @brief Linux 后端的薄转发头，复用已有 POSIX 实现，避免维护重复定义。
 * @details 转发 POSIX steady_clock 毫秒时钟，不另外声明 Linux 专属时间域。
 * 相对包含路径由源码目录结构决定；本头只选择实现，不创建对象或取得系统资源。
 * 阅读具体操作、错误处理及并发限制时继续进入被包含的 POSIX 头文件。
 */
// Linux OSAL chrono — delegates to the POSIX steady-clock implementation.
#pragma once
#include "../../posix/include/dp_osal_chrono.h"
