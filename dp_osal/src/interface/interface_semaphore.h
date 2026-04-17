/** @file interface_semaphore.h
 *  @brief CRTP semaphore interface for OSAL. */
#ifndef OSAL_INTERFACE_SEMAPHORE_H_
#define OSAL_INTERFACE_SEMAPHORE_H_

#include <cstdint>

namespace osal {

/** @brief CRTP base for semaphore implementations. */
template <typename Impl>
class SemaphoreBase {
public:
    void wait() { impl().doWait(); }
    void signal() { impl().doSignal(); }
    bool tryWait() { return impl().doTryWait(); }
    bool tryWaitFor(uint32_t timestamp) { return impl().doTryWaitFor(timestamp); }
    [[nodiscard]] int getValue() const { return impl().doGetValue(); }
    void init(int initialValue) { impl().doInit(initialValue); }

protected:
    ~SemaphoreBase() = default;

private:
    Impl &impl() { return *static_cast<Impl *>(this); }
    const Impl &impl() const { return *static_cast<const Impl *>(this); }
};

}  // namespace osal

#endif  // OSAL_INTERFACE_SEMAPHORE_H_
