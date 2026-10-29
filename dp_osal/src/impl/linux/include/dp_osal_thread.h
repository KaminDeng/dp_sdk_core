/**
 * @file dp_osal_thread.h
 * @brief Linux 后端的薄转发头，复用已有 POSIX 实现，避免维护重复定义。
 * @details 转发 POSIX pthread 线程实现，raw/owned 上下文规则及停止策略沿用该后端。
 * 相对包含路径由源码目录结构决定；本头只选择实现，不创建对象或取得系统资源。
 * 阅读具体操作、错误处理及并发限制时继续进入被包含的 POSIX 头文件。
 */
// core/dp_sdk_core/osal/src/impl/linux/include/osal_thread.h
// Linux OSAL thread — delegates to posix impl (pthread).
// pthread is available on all POSIX platforms including Linux.
#pragma once
#include "../../posix/include/dp_osal_thread.h"
