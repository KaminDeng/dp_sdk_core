//
// Created by kamin.deng on 2024/8/22.
//
#ifndef DP_OSAL_CMSIS_LOCKGUARD_H_
#define DP_OSAL_CMSIS_LOCKGUARD_H_

#include "interface_lockguard.h"
#include "dp_osal_mutex.h"

namespace dp::osal {

using LockGuard = LockGuardBase<Mutex>;

} // namespace dp::osal

#endif  // DP_OSAL_CMSIS_LOCKGUARD_H_
