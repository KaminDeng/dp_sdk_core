/** @file osal_static.h
 *  @brief OSAL static allocation helpers for bare-metal builds. */
#ifndef OSAL_STATIC_H_
#define OSAL_STATIC_H_

#include <new>
#include <type_traits>

#ifndef OSAL_ENABLE_STATIC_ALLOC
#define OSAL_ENABLE_STATIC_ALLOC 0
#endif

/** @brief Define a statically allocated OSAL object.
 *
 *  In static-alloc mode, the object is placement-new'ed into static storage.
 *  Otherwise, this expands to a regular local object definition.
 */
#if OSAL_ENABLE_STATIC_ALLOC
#define OSAL_STATIC_OBJECT_DEFINE(type_, name_, ...)                           \
    alignas(type_) unsigned char _osal_static_buf_##name_[sizeof(type_)];      \
    type_ &name_ = *new (_osal_static_buf_##name_) type_(__VA_ARGS__)
#else
#define OSAL_STATIC_OBJECT_DEFINE(type_, name_, ...) type_ name_(__VA_ARGS__)
#endif

/* Convenience aliases for frequently used primitives. */
#define OSAL_MUTEX_DEFINE(name_) OSAL_STATIC_OBJECT_DEFINE(::osal::OSALMutex, name_)
#define OSAL_SEMAPHORE_DEFINE(name_, ...) OSAL_STATIC_OBJECT_DEFINE(::osal::OSALSemaphore, name_, __VA_ARGS__)

#endif  // OSAL_STATIC_H_
