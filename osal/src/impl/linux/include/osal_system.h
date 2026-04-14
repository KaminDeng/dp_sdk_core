// core/dp_sdk_core/osal/src/impl/linux/include/osal_system.h
// Linux OSAL system — delegates to posix impl.
// The posix OSALSystem already has Linux-specific /proc support compiled in
// via #if defined(__linux__) guards in osal_system.h.
#pragma once
#include "../../posix/include/osal_system.h"
