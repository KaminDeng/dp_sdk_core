//
// Created by kamin.deng on 2024/8/23.
//
#ifndef __OSAL_LOCK_GUARD_H__
#define __OSAL_LOCK_GUARD_H__

#include "interface_lockguard.h"
#include "osal_mutex.h"

namespace osal {

using OSALLockGuard = LockGuard<OSALMutex>;

}  // namespace osal

#endif  // __OSAL_LOCK_GUARD_H__
