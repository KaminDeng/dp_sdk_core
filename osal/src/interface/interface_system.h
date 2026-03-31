/** @file interface_system.h
 *  @brief Abstract system/scheduler interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ISYSTEM_H_
#define ISYSTEM_H_

#include <cstdint> /* uint32_t — required before any platform-specific includes */

namespace osal {

/** @brief Abstract system interface providing scheduler control and sleep.
 *
 *  @c StartScheduler() launches the RTOS scheduler on embedded targets; on
 *  POSIX it is a no-op.  @c sleep_ms() and @c sleep() allow OS-portable
 *  blocking delays without exposing platform-specific APIs. */
class ISystem {
public:
    /** @brief Starts the RTOS scheduler.
     *
     *  On bare-metal RTOS targets this function never returns.  On POSIX
     *  (posix_native port) this is a no-op and returns immediately. */
    virtual void StartScheduler() {}

    /** @brief Suspends the calling thread for the given number of milliseconds.
     *  @param milliseconds  Sleep duration in milliseconds. */
    virtual void sleep_ms(uint32_t milliseconds) const = 0;

    /** @brief Suspends the calling thread for the given number of seconds.
     *  @param seconds  Sleep duration in seconds. */
    virtual void sleep(uint32_t seconds) const = 0;

    /** @brief Returns a human-readable string describing the underlying OS/RTOS.
     *  @return Null-terminated platform description string. */
    virtual const char *get_system_info() const = 0;
};

}  // namespace osal
#endif  // ISYSTEM_H_
