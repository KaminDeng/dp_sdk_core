//
// Created by kamin.deng on 2024/8/23.
//
#ifndef OSAL_POSIX_LOCKGUARD_H_
#define OSAL_POSIX_LOCKGUARD_H_

#include "interface_lockguard.h"
#include "osal_mutex.h"

namespace osal {

using OSALLockGuard = LockGuard<OSALMutex>;

}  // namespace osal

#endif  // OSAL_POSIX_LOCKGUARD_H_
