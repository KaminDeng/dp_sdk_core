/**
 * @file dp_osal_host_io_fault_tests.cpp
 * @brief Linux独立进程中的Host能力故障注入；只在实际支持GNU wrap时装配。
 */
#include "dp_osal.h"
#include "dp_osal_file.h"
#include "dp_osal_file_system.h"
#include "dp_osal_native_io.h"
#include "dp_osal_native_process.h"
#include "dp_osal_chrono.h"
#include "dp_osal_semaphore.h"
#include <gtest/gtest.h>
#include <cerrno>
#include <csignal>
#include <climits>
#include <poll.h>
#include <semaphore.h>
#include <unistd.h>

namespace {
enum class WaitMode { kNative, kInterruptTwice, kDeadlineInterrupt, kError, kPreciseClock };
bool fail_init = false, fail_clock = false, fail_destroy = false, fail_term = false;
bool initialize_full = false;
WaitMode wait_mode = WaitMode::kNative;
unsigned waits = 0, clocks = 0, reads = 0, destroys = 0;
short input_events = 0;
int input_read_error = EIO;
bool interrupt_poll = false;
timespec first_deadline{};
bool same_deadline = true;
bool interrupt_write = false, interrupt_pread = false, interrupt_pwrite = false;
bool fail_sync = false, fail_close = false, fail_restore_mask = false;
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
        input_read_error = EIO;
        interrupt_poll = false;
        same_deadline = true;
        interrupt_write = interrupt_pread = interrupt_pwrite = false;
        fail_sync = fail_close = fail_restore_mask = false;
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
        output->tv_nsec = wait_mode == WaitMode::kPreciseClock
                              ? 123456789L
                              : (clocks == 0 ? 0 : (wait_mode == WaitMode::kDeadlineInterrupt ? 50000000L : 1000000L));
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
    if (interrupt_poll) {
        errno = EINTR;
        return -1;
    }
    if (input_events != 0 && count == 1 && descriptors[0].fd == STDIN_FILENO) {
        descriptors[0].revents = input_events;
        return 1;
    }
    return __real_poll(descriptors, count, timeout);
}
extern "C" ssize_t __wrap_read(int descriptor, void* output, size_t size) {
    if (input_events != 0 && descriptor == STDIN_FILENO) {
        ++reads;
        errno = input_read_error;
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
    dp::osal::Semaphore wake{dp::osal::Semaphore::DeferredInit{}};
    fail_init = true;
    EXPECT_FALSE(wake.open());
    EXPECT_FALSE(wake.release());
    EXPECT_EQ(wake.waitForMs(0), dp::osal::WaitResult::kError);
    EXPECT_TRUE(wake.close());
    EXPECT_EQ(destroys, 0U);
    fail_init = false;
    ASSERT_TRUE(wake.open());
    EXPECT_FALSE(wake.open());
    EXPECT_TRUE(wake.close());
    EXPECT_EQ(destroys, 1U);
}

TEST_F(HostNativeFailure, FullCounterReportsFailureWithoutClosing) {
    dp::osal::Semaphore wake{dp::osal::Semaphore::DeferredInit{}};
    initialize_full = true;
    ASSERT_TRUE(wake.open(SEM_VALUE_MAX));
    EXPECT_FALSE(wake.release());
    EXPECT_EQ(errno, EOVERFLOW);
    EXPECT_EQ(wake.waitForMs(0), dp::osal::WaitResult::kAcquired);
    EXPECT_TRUE(wake.release());
}

TEST_F(HostNativeFailure, InterruptsReuseOriginalMonotonicDeadline) {
    dp::osal::Semaphore wake{dp::osal::Semaphore::DeferredInit{}};
    ASSERT_TRUE(wake.open());
    wait_mode = WaitMode::kInterruptTwice;
    EXPECT_EQ(wake.waitForMs(50), dp::osal::WaitResult::kTimeout);
    EXPECT_EQ(waits, 3U);
    EXPECT_EQ(clocks, 3U);
    EXPECT_TRUE(same_deadline);
    EXPECT_EQ(first_deadline.tv_sec, 100);
    EXPECT_EQ(first_deadline.tv_nsec, 50000000L);
}

TEST_F(HostNativeFailure, InterruptedWaitAtDeadlineDoesNotRestartBudget) {
    dp::osal::Semaphore wake{dp::osal::Semaphore::DeferredInit{}};
    ASSERT_TRUE(wake.open());
    wait_mode = WaitMode::kDeadlineInterrupt;
    EXPECT_EQ(wake.waitForMs(50), dp::osal::WaitResult::kTimeout);
    EXPECT_EQ(waits, 1U);
    EXPECT_EQ(clocks, 2U);
}

TEST_F(HostNativeFailure, NativeClockAndWaitErrorsRemainDistinctFromTimeout) {
    dp::osal::Semaphore wake{dp::osal::Semaphore::DeferredInit{}};
    ASSERT_TRUE(wake.open());
    fail_clock = true;
    EXPECT_EQ(wake.waitForMs(0), dp::osal::WaitResult::kError);
    fail_clock = false;
    wait_mode = WaitMode::kError;
    EXPECT_EQ(wake.waitForMs(0), dp::osal::WaitResult::kError);
}

TEST_F(HostNativeFailure, FailedCloseDoesNotReuseDestroyedSemaphore) {
    dp::osal::Semaphore wake{dp::osal::Semaphore::DeferredInit{}};
    ASSERT_TRUE(wake.open());
    fail_destroy = true;
    EXPECT_FALSE(wake.close());
    EXPECT_TRUE(wake.close());
    EXPECT_EQ(destroys, 1U);
    EXPECT_FALSE(wake.release());
    EXPECT_EQ(wake.waitForMs(0), dp::osal::WaitResult::kError);
}

TEST_F(HostNativeFailure, PollErrorsNeverAttemptPotentiallyBlockingRead) {
    for (const short events : {static_cast<short>(POLLNVAL), static_cast<short>(POLLERR)}) {
        input_events = events;
        dp::osal::NativeInputInfo info{};
        uint8_t value = 9;
        EXPECT_EQ(dp::osal::NativeProcess::pollInput(&value, &info), dp::osal::IoResult::kError);
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
    EXPECT_FALSE(dp::osal::NativeProcess::installTerminationHandler(requested_handler, true));
    struct sigaction actual {};
    ASSERT_EQ(::sigaction(SIGINT, nullptr, &actual), 0);
    EXPECT_EQ(actual.sa_handler, previous_handler);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST_F(HostNativeFailure, CheckedChronoRejectsUnavailableMonotonicSource) {
    fail_clock = true;
    EXPECT_EQ(dp::osal::Chrono::steadyNs(), 0U);
    fail_clock = false;
    EXPECT_GT(dp::osal::Chrono::steadyNs(), 0U);
    wait_mode = WaitMode::kPreciseClock;
    EXPECT_EQ(dp::osal::Chrono::steadyNs(), UINT64_C(100123456789));
}

extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __real_pread(int, void*, size_t, off_t);
extern "C" ssize_t __real_pwrite(int, const void*, size_t, off_t);
extern "C" int __real_fsync(int);
extern "C" int __real_close(int);
extern "C" int __real_pthread_sigmask(int, const sigset_t*, sigset_t*);
extern "C" ssize_t __wrap_write(int descriptor, const void* bytes, size_t size) {
    if (interrupt_write) {
        errno = EINTR;
        return -1;
    }
    return __real_write(descriptor, bytes, size);
}
extern "C" ssize_t __wrap_pread(int descriptor, void* bytes, size_t size, off_t offset) {
    if (interrupt_pread) {
        errno = EINTR;
        return -1;
    }
    return __real_pread(descriptor, bytes, size, offset);
}
extern "C" ssize_t __wrap_pwrite(int descriptor, const void* bytes, size_t size, off_t offset) {
    if (interrupt_pwrite) {
        errno = EINTR;
        return -1;
    }
    return __real_pwrite(descriptor, bytes, size, offset);
}
extern "C" int __wrap_fsync(int descriptor) {
    if (fail_sync) {
        errno = EIO;
        return -1;
    }
    return __real_fsync(descriptor);
}
extern "C" int __wrap_close(int descriptor) {
    const int result = __real_close(descriptor);
    if (fail_close) {
        errno = EIO;
        return -1;
    }
    return result;
}
extern "C" int __wrap_pthread_sigmask(int how, const sigset_t* requested, sigset_t* previous) {
    const int result = __real_pthread_sigmask(how, requested, previous);
    // 已恢复真实mask后报告失败，验证部分成功且不污染其它测试线程的信号策略。
    if (how == SIG_SETMASK && fail_restore_mask) return EINVAL;
    return result;
}

TEST_F(HostNativeFailure, FileInterruptAndPersistenceFailuresKeepExactClassification) {
    char path[] = "/tmp/dp-osal-checked-file-XXXXXX";
    const int borrowed = ::mkstemp(path);
    ASSERT_GE(borrowed, 0);
    struct Remove {
        const char* path;
        int borrowed;
        ~Remove() {
            (void)::close(borrowed);
            (void)::unlink(path);
        }
    } remove{path, borrowed};
    dp::osal::File file;
    ASSERT_TRUE(file.openExisting(path));
    uint8_t byte = 42U;
    size_t count = 99U;
    interrupt_write = true;
    EXPECT_EQ(file.tryWrite(&byte, 1U, &count), dp::osal::IoResult::kWouldBlock);
    EXPECT_EQ(count, 0U);
    interrupt_write = false;
    interrupt_pwrite = true;
    EXPECT_EQ(file.writeAt(&byte, 1U, 0U, &count), dp::osal::IoResult::kWouldBlock);
    EXPECT_EQ(count, 0U);
    interrupt_pwrite = false;
    ASSERT_EQ(file.writeAt(&byte, 1U, 0U, &count), dp::osal::IoResult::kOk);
    interrupt_pread = true;
    EXPECT_EQ(file.readAt(&byte, 1U, 0U, &count), dp::osal::IoResult::kWouldBlock);
    EXPECT_EQ(count, 0U);
    interrupt_pread = false;
    EXPECT_EQ(file.readAt(&byte, 1U, 1U, &count), dp::osal::IoResult::kOk);
    EXPECT_EQ(count, 0U);  // EOF不是关闭或错误。
    fail_sync = true;
    EXPECT_FALSE(file.sync());
    fail_sync = false;
    EXPECT_TRUE(file.sync());
    fail_close = true;
    EXPECT_FALSE(file.close());
    fail_close = false;
    EXPECT_TRUE(file.close());
    EXPECT_EQ(file.tryWrite(&byte, 1U, &count), dp::osal::IoResult::kClosed);
    EXPECT_GE(::fcntl(borrowed, F_GETFD), 0);
}

TEST_F(HostNativeFailure, MaskRestoreFailureDoesNotProveThatNoBytesWereWritten) {
    char path[] = "/tmp/dp-osal-checked-mask-XXXXXX";
    const int borrowed = ::mkstemp(path);
    ASSERT_GE(borrowed, 0);
    struct Remove {
        const char* path;
        int borrowed;
        ~Remove() {
            (void)::close(borrowed);
            (void)::unlink(path);
        }
    } remove{path, borrowed};
    dp::osal::ByteOutput output;
    ASSERT_TRUE(dp::osal::NativeIo::openOutput(output, borrowed));
    const uint8_t sent = 37U;
    size_t count = 99U;
    fail_restore_mask = true;
    EXPECT_EQ(output.tryWrite(&sent, 1U, &count), dp::osal::IoResult::kError);
    EXPECT_EQ(count, 0U);
    fail_restore_mask = false;
    uint8_t actual{};
    ASSERT_EQ(::pread(borrowed, &actual, 1U, 0), 1);
    EXPECT_EQ(actual, sent);
}

TEST_F(HostNativeFailure, InputPollAndReadInterruptsPreserveTheirDistinctResults) {
    uint8_t byte = 41U;
    dp::osal::NativeInputInfo info{};
    interrupt_poll = true;
    EXPECT_EQ(dp::osal::NativeProcess::pollInput(&byte, &info), dp::osal::IoResult::kWouldBlock);
    EXPECT_EQ(info.error, EINTR);
    EXPECT_FALSE(info.read_attempted);
    EXPECT_EQ(byte, 41U);
    interrupt_poll = false;
    input_events = POLLIN;
    input_read_error = EINTR;
    EXPECT_EQ(dp::osal::NativeProcess::pollInput(&byte, &info), dp::osal::IoResult::kError);
    EXPECT_EQ(info.error, EINTR);
    EXPECT_TRUE(info.read_attempted);
    EXPECT_EQ(byte, 41U);
}

// glibc FORTIFY可把poll调用路由到此别名；仍注入同一真实叶层，不能漏测错误路径。
extern "C" int __wrap___poll_chk(pollfd* descriptors, nfds_t count, int timeout, size_t bytes) {
    if (count > bytes / sizeof(pollfd)) {
        errno = EINVAL;
        return -1;
    }
    return __wrap_poll(descriptors, count, timeout);
}
extern "C" ssize_t __wrap___read_chk(int descriptor, void* bytes, size_t size, size_t capacity) {
    if (size > capacity) {
        errno = EINVAL;
        return -1;
    }
    return __wrap_read(descriptor, bytes, size);
}
extern "C" ssize_t __wrap___pread_chk(int descriptor, void* bytes, size_t size, off_t offset, size_t capacity) {
    if (size > capacity) {
        errno = EINVAL;
        return -1;
    }
    return __wrap_pread(descriptor, bytes, size, offset);
}
