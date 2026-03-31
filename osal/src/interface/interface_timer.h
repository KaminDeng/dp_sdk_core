/** @file interface_timer.h
 *  @brief Abstract one-shot and periodic timer interface for OSAL. */
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef ITIMER_H_
#define ITIMER_H_

#include <functional>

namespace osal {

/** @brief Abstract software timer interface.
 *
 *  Supports both one-shot and periodic callbacks.  Concrete implementations
 *  use @c timer_create / @c SIGEV_THREAD (POSIX) or an RTOS software-timer
 *  API.  The callback is invoked from the platform timer thread or ISR
 *  context, so it must be kept short and ISR-safe when on embedded targets. */
class ITimer {
public:
    virtual ~ITimer() {}

    /** @brief Configures and starts the timer.
     *  @param interval  Timer period in milliseconds.
     *  @param periodic  @c true for a repeating timer, @c false for a one-shot.
     *  @param callback  Function invoked on each expiry. */
    virtual void start(uint32_t interval, bool periodic, std::function<void()> callback) = 0;

    /** @brief Stops the timer, cancelling any pending expiry. */
    virtual void stop() = 0;

    /** @brief Checks whether the timer is currently running.
     *  @return @c true if the timer is active, @c false otherwise. */
    virtual bool isRunning() const = 0;

    /** @brief Returns the time remaining until the next expiry.
     *  @return Remaining time in milliseconds, or 0 if not running. */
    virtual uint32_t getRemainingTime() const = 0;

    /** @brief Restarts the timer from its configured interval. */
    virtual void reset() = 0;
};

}  // namespace osal
#endif  // ITIMER_H_
