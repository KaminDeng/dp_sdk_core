/** @file dp_osal_thread_stop.h
 * @brief CMSIS 当前任务的协作停止标志；不会通过异常或强制取消退出业务。
 */
#pragma once
#include <atomic>
#include <cstdint>
#ifdef DPSDK_MCU_FIRMWARE
#include "FreeRTOS.h"
#include "task.h"
#endif
namespace dp::osal {
namespace detail {
/** @brief 毫秒向上换算tick，64位中间值避免乘法回绕；调用方检查端口可表示范围。 */
constexpr uint64_t cmsisMillisecondsToTicks(uint32_t milliseconds, uint32_t frequency) noexcept {
    return (static_cast<uint64_t>(milliseconds) * frequency + 999ULL) / 1000ULL;
}
}  // namespace detail
#ifdef DPSDK_MCU_FIRMWARE
inline std::atomic<bool>* osal_cmsis_stop_flag_get() {
    return static_cast<std::atomic<bool>*>(pvTaskGetThreadLocalStoragePointer(nullptr, 0));
}
inline void osal_cmsis_stop_flag_set(std::atomic<bool>* flag) { vTaskSetThreadLocalStoragePointer(nullptr, 0, flag); }
#else
inline thread_local std::atomic<bool>* tl_cmsis_stop_flag_storage = nullptr;
inline std::atomic<bool>* osal_cmsis_stop_flag_get() { return tl_cmsis_stop_flag_storage; }
inline void osal_cmsis_stop_flag_set(std::atomic<bool>* flag) { tl_cmsis_stop_flag_storage = flag; }
#endif
}  // namespace dp::osal
