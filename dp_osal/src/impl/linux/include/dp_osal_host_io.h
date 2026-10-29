/**
 * @file dp_osal_host_io.h
 * @brief Linux 后端的薄转发头，复用已有 POSIX 实现，避免维护重复定义。
 * @details 转发 POSIX 目录中的 Linux 专用主机 I/O 实现；当前实现依赖 proc、poll 和 POSIX 描述符。
 * 相对包含路径由源码目录结构决定；本头只选择实现，不创建对象或取得系统资源。
 * 阅读具体操作、错误处理及并发限制时继续进入被包含的 POSIX 头文件。
 */
#pragma once
#include "../../posix/include/dp_osal_host_io.h"
