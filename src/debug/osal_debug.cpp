//
// Created by kamin.deng on 2024/8/23.
//
#include "osal_debug.h"

namespace osal {

// 当前日志等级
static LogLevel currentLogLevel = LOG_LEVEL_INFO;

// 是否输出文件名、函数名和行数
static bool includeFileFunctionLine = false;

// 设置日志等级
void setLogLevel(LogLevel level) { currentLogLevel = level; }

LogLevel getLogLevel() { return currentLogLevel; }

// 设置是否输出文件名、函数名和行数
void setIncludeFileFunctionLine(bool include) { includeFileFunctionLine = include; }

bool getIncludeFileFunction() { return includeFileFunctionLine; }

// 通用日志函数
void common_log(const char *prefix, const char *file, const char *function, int line, const char *format,
                va_list args) {
    char buf[512];
    char *ptr = buf;

    // 构建前缀
    if (getIncludeFileFunction()) {
        size_t rem = sizeof(buf) - (size_t)(ptr - buf);
        ptr += snprintf(ptr, rem, "[%s:%s:%d] ", file, function, line);
    }

    // 复制prefix到buf中（如果prefix非空且还有空间）
    if (prefix != nullptr && *prefix != '\0') {
        size_t rem = sizeof(buf) - (size_t)(ptr - buf);
        if (rem > 0) {
            ptr += snprintf(ptr, rem, "%s", prefix);
        }
    }

    int written = 0;
    // 如果prefix和前缀信息之后还有空间，则格式化日志消息
    {
        size_t rem = sizeof(buf) - (size_t)(ptr - buf);
        if (rem > 0) {
            va_list args_copy;
            va_copy(args_copy, args);
            written = vsnprintf(ptr, rem, format, args_copy);
            va_end(args_copy);
        }
    }

    // 计算实际需要发送的字节数
    size_t prefix_len = (size_t)(ptr - buf);
    size_t msg_len    = (written > 0) ? (size_t)written : 0;
    uint32_t total    = (uint32_t)(prefix_len + msg_len);

    // 输出日志
    osal_port_debug_write(buf, total);
}

// 定义具体的日志函数
DEFINE_LOG_FUNCTION(OSAL_LOG_, "", LOG_LEVEL_INFO)

DEFINE_LOG_FUNCTION(OSAL_LOGA_, "LOGA: ", LOG_LEVEL_VERBOSE)

DEFINE_LOG_FUNCTION(OSAL_LOGE_, "LOGE: ", LOG_LEVEL_ERROR)

DEFINE_LOG_FUNCTION(OSAL_LOGW_, "LOGW: ", LOG_LEVEL_WARNING)

DEFINE_LOG_FUNCTION(OSAL_LOGI_, "LOGI: ", LOG_LEVEL_INFO)

DEFINE_LOG_FUNCTION(OSAL_LOGD_, "LOGD: ", LOG_LEVEL_DEBUG)

DEFINE_LOG_FUNCTION(OSAL_LOGV_, "LOGV: ", LOG_LEVEL_VERBOSE)

}  // namespace osal
