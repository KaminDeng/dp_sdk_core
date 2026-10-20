/**
 * @file dp_osal_host_console_stub.cpp
 * @brief 独立主机测试的空控制台链接桩，避免引入完整设备/日志装配。
 * @details 所有输出都被丢弃，因此这些目标不能凭控制台文本验证产品日志内容。
 */
#include <cstddef>

/**
 * @brief 满足dp_console_write的C链接符号，忽略缓冲与字节数，不执行I/O。
 * @details 两个参数均未命名且不解引用；这是测试目标链接桩，不能作为产品输出实现。
 */
extern "C" void dp_console_write(const char*, size_t) {}
