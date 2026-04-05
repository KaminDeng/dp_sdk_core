/** @file osal_task_snapshot.h
 *  @brief Portable OSAL task snapshot schema for shell/monitoring use. */
#ifndef OSAL_TASK_SNAPSHOT_H_
#define OSAL_TASK_SNAPSHOT_H_

#include <cstdint>

namespace osal {

struct TaskSnapshot {
    char name[16];
    uint32_t cpu_pct_x10;
    uint32_t stack_hwm;
    uint8_t state;
};

}  // namespace osal

#endif  // OSAL_TASK_SNAPSHOT_H_
