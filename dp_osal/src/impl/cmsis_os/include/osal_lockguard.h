//
// Created by kamin.deng on 2024/8/22.
//
#ifndef OSAL_CMSIS_LOCKGUARD_H_
#define OSAL_CMSIS_LOCKGUARD_H_

#include "interface_lockguard.h"
#include "osal_mutex.h"

namespace osal {

using OSALLockGuard = LockGuard<OSALMutex>;

}  // namespace osal

#endif  // OSAL_CMSIS_LOCKGUARD_H_
