#include <gtest/gtest.h>

#include "interface_mutex.h"
#include "osal_virtual.h"

using namespace osal;

namespace {

class FakeMutex : public MutexBase<FakeMutex> {
    friend class MutexBase<FakeMutex>;

public:
    bool lock_result = true;
    bool unlock_result = true;
    int lock_calls = 0;
    int unlock_calls = 0;
    int try_lock_calls = 0;
    int try_lock_for_calls = 0;
    uint32_t last_timeout_ms = 0;

private:
    bool doLock() {
        ++lock_calls;
        return lock_result;
    }

    bool doUnlock() {
        ++unlock_calls;
        return unlock_result;
    }

    bool doTryLock() {
        ++try_lock_calls;
        return lock_result;
    }

    bool doTryLockFor(uint32_t timeout_ms) {
        ++try_lock_for_calls;
        last_timeout_ms = timeout_ms;
        return lock_result;
    }
};

class CounterService {
public:
    explicit CounterService(IMutex &mutex) : mutex_(mutex) {}

    bool increment() {
        if (!mutex_.lock()) {
            return false;
        }
        ++value_;
        (void)mutex_.unlock();
        return true;
    }

    [[nodiscard]] int value() const { return value_; }

private:
    IMutex &mutex_;
    int value_ = 0;
};

}  // namespace

// Host 注入示例：业务代码依赖 IMutex，由 MutexVirtual<FakeMutex> 注入实现。
TEST(OSALVirtualInjectionTest, MutexVirtualInjectsHostFakeIntoService) {
#if (OSAL_TEST_MUTEX_ENABLED || OSAL_TEST_ALL)
    FakeMutex fake;
    MutexVirtual<FakeMutex> injected(fake);
    CounterService service(injected);

    EXPECT_TRUE(service.increment());
    EXPECT_EQ(service.value(), 1);
    EXPECT_EQ(fake.lock_calls, 1);
    EXPECT_EQ(fake.unlock_calls, 1);
#else
    GTEST_SKIP();
#endif
}

TEST(OSALVirtualInjectionTest, MutexVirtualPropagatesFailureFromInjectedFake) {
#if (OSAL_TEST_MUTEX_ENABLED || OSAL_TEST_ALL)
    FakeMutex fake;
    fake.lock_result = false;
    MutexVirtual<FakeMutex> injected(fake);
    CounterService service(injected);

    EXPECT_FALSE(service.increment());
    EXPECT_EQ(service.value(), 0);
    EXPECT_EQ(fake.lock_calls, 1);
    EXPECT_EQ(fake.unlock_calls, 0);
#else
    GTEST_SKIP();
#endif
}
