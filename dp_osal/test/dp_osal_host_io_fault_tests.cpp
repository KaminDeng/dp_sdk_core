/**
 * @file dp_osal_host_io_fault_tests.cpp
 * @brief Linux独立进程中的Host能力故障注入；只在实际支持GNU wrap时装配。
 */
#include "dp_osal.h"
#include "dp_osal_host_io.h"
#include "dp_osal_host_wake.h"
#include <gtest/gtest.h>
#include <cerrno>
#include <csignal>
#include <climits>
#include <poll.h>
#include <semaphore.h>
#include <unistd.h>

namespace {
enum class WaitMode { kNative, kInterruptTwice, kDeadlineInterrupt, kError };
bool fail_init = false, fail_clock = false, fail_destroy = false, fail_term = false;
bool initialize_full = false;
WaitMode wait_mode = WaitMode::kNative;
unsigned waits = 0, clocks = 0, reads = 0, destroys = 0;
short input_events = 0;
timespec first_deadline{};
bool same_deadline = true;
using Handler = void (*)(int);
void previous_handler(int) {}
void requested_handler(int) {}
class HostNativeFailure : public ::testing::Test {
    void TearDown() override {
        fail_init = fail_clock = fail_destroy = fail_term = false;
        initialize_full = false;
        wait_mode = WaitMode::kNative;
        waits = clocks = reads = destroys = 0;
        input_events = 0;
        same_deadline = true;
    }
};
}  // namespace

extern "C" int __real_sem_init(sem_t*, int, unsigned);
extern "C" int __real_sem_destroy(sem_t*);
extern "C" int __real_sem_clockwait(sem_t*, clockid_t, const timespec*);
extern "C" int __real_clock_gettime(clockid_t, timespec*);
extern "C" int __real_poll(pollfd*, nfds_t, int);
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" Handler __real_signal(int, Handler);

extern "C" int __wrap_sem_init(sem_t* value, int shared, unsigned count) {
    if (fail_init) {
        errno = ENOSPC;
        return -1;
    }
    return __real_sem_init(value, shared, initialize_full ? SEM_VALUE_MAX : count);
}
extern "C" int __wrap_sem_destroy(sem_t* value) {
    ++destroys;
    const int result = __real_sem_destroy(value);
    if (fail_destroy) {
        errno = EIO;
        return -1;
    }
    return result;
}
extern "C" int __wrap_clock_gettime(clockid_t clock, timespec* output) {
    if (clock == CLOCK_MONOTONIC && fail_clock) {
        errno = EIO;
        return -1;
    }
    if (clock == CLOCK_MONOTONIC && wait_mode != WaitMode::kNative) {
        output->tv_sec = 100;
        output->tv_nsec = clocks == 0 ? 0 : (wait_mode == WaitMode::kDeadlineInterrupt ? 50000000L : 1000000L);
        ++clocks;
        return 0;
    }
    return __real_clock_gettime(clock, output);
}
extern "C" int __wrap_sem_clockwait(sem_t* value, clockid_t clock, const timespec* deadline) {
    if (wait_mode == WaitMode::kNative) return __real_sem_clockwait(value, clock, deadline);
    if (waits++ == 0) first_deadline = *deadline;
    same_deadline = same_deadline && first_deadline.tv_sec == deadline->tv_sec &&
                    first_deadline.tv_nsec == deadline->tv_nsec && clock == CLOCK_MONOTONIC;
    errno = wait_mode == WaitMode::kError
                ? EINVAL
                : (wait_mode == WaitMode::kDeadlineInterrupt || waits <= 2 ? EINTR : ETIMEDOUT);
    return -1;
}
extern "C" int __wrap_poll(pollfd* descriptors, nfds_t count, int timeout) {
    if (input_events != 0 && count == 1 && descriptors[0].fd == STDIN_FILENO) {
        descriptors[0].revents = input_events;
        return 1;
    }
    return __real_poll(descriptors, count, timeout);
}
extern "C" ssize_t __wrap_read(int descriptor, void* output, size_t size) {
    if (input_events != 0 && descriptor == STDIN_FILENO) {
        ++reads;
        errno = EIO;
        return -1;
    }
    return __real_read(descriptor, output, size);
}
extern "C" Handler __wrap_signal(int number, Handler handler) {
    if (fail_term && number == SIGTERM) {
        errno = EIO;
        return SIG_ERR;
    }
    return __real_signal(number, handler);
}

TEST_F(HostNativeFailure, InitializationFailureDoesNotAcquireAndMayRetry) {
    dp::osal::OSALHostWake wake;
    fail_init = true;
    EXPECT_FALSE(wake.open());
    EXPECT_FALSE(wake.signal());
    EXPECT_EQ(wake.wait_for_ms(0), dp::osal::HostWakeResult::kError);
    EXPECT_TRUE(wake.close());
    EXPECT_EQ(destroys, 0U);
    fail_init = false;
    ASSERT_TRUE(wake.open());
    EXPECT_FALSE(wake.open());
    EXPECT_TRUE(wake.close());
    EXPECT_EQ(destroys, 1U);
}

TEST_F(HostNativeFailure, FullCounterReportsFailureWithoutClosing) {
    dp::osal::OSALHostWake wake;
    initialize_full = true;
    ASSERT_TRUE(wake.open());
    EXPECT_FALSE(wake.signal());
    EXPECT_EQ(errno, EOVERFLOW);
    EXPECT_EQ(wake.wait_for_ms(0), dp::osal::HostWakeResult::kSignaled);
    EXPECT_TRUE(wake.signal());
}

TEST_F(HostNativeFailure, InterruptsReuseOriginalMonotonicDeadline) {
    dp::osal::OSALHostWake wake;
    ASSERT_TRUE(wake.open());
    wait_mode = WaitMode::kInterruptTwice;
    EXPECT_EQ(wake.wait_for_ms(50), dp::osal::HostWakeResult::kTimeout);
    EXPECT_EQ(waits, 3U);
    EXPECT_EQ(clocks, 3U);
    EXPECT_TRUE(same_deadline);
    EXPECT_EQ(first_deadline.tv_sec, 100);
    EXPECT_EQ(first_deadline.tv_nsec, 50000000L);
}

TEST_F(HostNativeFailure, InterruptedWaitAtDeadlineDoesNotRestartBudget) {
    dp::osal::OSALHostWake wake;
    ASSERT_TRUE(wake.open());
    wait_mode = WaitMode::kDeadlineInterrupt;
    EXPECT_EQ(wake.wait_for_ms(50), dp::osal::HostWakeResult::kTimeout);
    EXPECT_EQ(waits, 1U);
    EXPECT_EQ(clocks, 2U);
}

TEST_F(HostNativeFailure, NativeClockAndWaitErrorsRemainDistinctFromTimeout) {
    dp::osal::OSALHostWake wake;
    ASSERT_TRUE(wake.open());
    fail_clock = true;
    EXPECT_EQ(wake.wait_for_ms(0), dp::osal::HostWakeResult::kError);
    fail_clock = false;
    wait_mode = WaitMode::kError;
    EXPECT_EQ(wake.wait_for_ms(0), dp::osal::HostWakeResult::kError);
}

TEST_F(HostNativeFailure, FailedCloseDoesNotReuseDestroyedSemaphore) {
    dp::osal::OSALHostWake wake;
    ASSERT_TRUE(wake.open());
    fail_destroy = true;
    EXPECT_FALSE(wake.close());
    EXPECT_TRUE(wake.close());
    EXPECT_EQ(destroys, 1U);
    EXPECT_FALSE(wake.signal());
    EXPECT_EQ(wake.wait_for_ms(0), dp::osal::HostWakeResult::kError);
}

TEST_F(HostNativeFailure, PollErrorsNeverAttemptPotentiallyBlockingRead) {
    for (const short events : {static_cast<short>(POLLNVAL), static_cast<short>(POLLERR)}) {
        input_events = events;
        dp::osal::HostInputInfo info{};
        uint8_t value = 9;
        EXPECT_EQ(dp::osal::OSALHostIo::poll_input(&value, &info), dp::osal::HostIoResult::kError);
        EXPECT_NE(info.error, 0);
        EXPECT_FALSE(info.read_attempted);
        EXPECT_EQ(value, 9);
    }
    EXPECT_EQ(reads, 0U);
}

TEST_F(HostNativeFailure, FailedTerminationInstallRestoresPreviousInterruptHandler) {
    const Handler previous = __real_signal(SIGINT, previous_handler);
    ASSERT_NE(previous, SIG_ERR);
    struct Restore {
        Handler handler;
        ~Restore() { (void)__real_signal(SIGINT, handler); }
    } restore{previous};
    fail_term = true;
    EXPECT_FALSE(dp::osal::OSALHostIo::install_termination_handler(requested_handler, true));
    struct sigaction actual {};
    ASSERT_EQ(::sigaction(SIGINT, nullptr, &actual), 0);
    EXPECT_EQ(actual.sa_handler, previous_handler);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
