/** @file osal_thread_snapshot.h
 *  @brief Portable OSAL thread snapshot schema for shell/monitoring use. */
#ifndef OSAL_THREAD_SNAPSHOT_H_
#define OSAL_THREAD_SNAPSHOT_H_

#include <cstdint>

namespace osal {

struct ThreadSnapshot {
    char name[16];
    uint32_t cpu_pct_x10;
    uint32_t stack_hwm;
    uint8_t state;
    uint32_t stack_pointer;  ///< 任务保存的栈指针（预缓存用，0 = 未知）
};

}  // namespace osal

#endif  // OSAL_THREAD_SNAPSHOT_H_
