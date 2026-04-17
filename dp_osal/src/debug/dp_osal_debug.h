//
// Created by kamin.deng on 2024/8/23.
//
#ifndef DP_OSAL_DEBUG_H_
#define DP_OSAL_DEBUG_H_

#include <stdarg.h>
#include <stdio.h>

#include "dp_osal.h"

/* ── Portable compiler-attribute macros (self-contained, no external deps) ──
 * DP_OSAL_PRINTF_LIKE(fmt_idx, args_idx): annotate a variadic function so the
 * compiler validates format strings.  Indices are 1-based; use 0 for va_list
 * functions.  Falls back to empty on unsupported toolchains.               */
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif
#if __has_attribute(format) || defined(__GNUC__)
#define DP_OSAL_PRINTF_LIKE(fmt_idx, args_idx) __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define DP_OSAL_PRINTF_LIKE(fmt_idx, args_idx)
#endif

/* DP_OSAL_UNUSED: suppress "unused variable / parameter" warnings.            */
#if __has_attribute(unused) || defined(__GNUC__)
#define DP_OSAL_UNUSED __attribute__((unused))
#else
#define DP_OSAL_UNUSED
#endif

namespace dp::osal {

// 日志等级定义
enum LogLevel {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
};

void setLogLevel(LogLevel level);

LogLevel getLogLevel();

void setIncludeFileFunctionLine(bool include);

bool getIncludeFileFunction();

// 宏定义
#define DEFINE_LOG_FUNCTION(logFuncName, prefix, level)                                           \
    void logFuncName(const char* file, const char* function, int line, const char* format, ...) { \
        if (getLogLevel() >= level) {                                                             \
            va_list args;                                                                         \
            va_start(args, format);                                                               \
            common_log(prefix, file, function, line, format, args);                               \
            va_end(args);                                                                         \
        }                                                                                         \
    }

void common_log(const char* prefix, const char* file, const char* function, int line, const char* format, va_list args)
    DP_OSAL_PRINTF_LIKE(5, 0);

#define DECLARE_LOG_FUNCTION(logFuncName) \
    void logFuncName(const char* file, const char* function, int line, const char* format, ...) DP_OSAL_PRINTF_LIKE(4, 5);

DECLARE_LOG_FUNCTION(DP_OSAL_LOG_)

DECLARE_LOG_FUNCTION(DP_OSAL_LOGA_)

DECLARE_LOG_FUNCTION(DP_OSAL_LOGE_)

DECLARE_LOG_FUNCTION(DP_OSAL_LOGW_)

DECLARE_LOG_FUNCTION(DP_OSAL_LOGI_)

DECLARE_LOG_FUNCTION(DP_OSAL_LOGD_)

DECLARE_LOG_FUNCTION(DP_OSAL_LOGV_)

// 宏定义，用于简化日志调用
#define DP_OSAL_LOG(format, ...) DP_OSAL_LOG_(__FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define DP_OSAL_LOGA(format, ...) DP_OSAL_LOGA_(__FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define DP_OSAL_LOGE(format, ...) DP_OSAL_LOGE_(__FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define DP_OSAL_LOGW(format, ...) DP_OSAL_LOGW_(__FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define DP_OSAL_LOGI(format, ...) DP_OSAL_LOGI_(__FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define DP_OSAL_LOGD(format, ...) DP_OSAL_LOGD_(__FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define DP_OSAL_LOGV(format, ...) DP_OSAL_LOGV_(__FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

}  // namespace dp::osal

#endif  // DP_OSAL_DEBUG_H_
