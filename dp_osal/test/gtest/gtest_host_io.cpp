#include "dp_osal.h"
#if defined(DP_OSAL_BACKEND_POSIX) && defined(__linux__)
#include "dp_osal_host_io.h"
#include "dp_osal_host_wake.h"
#include <gtest/gtest.h>
#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>

TEST(OSALHostIo, FullPipeDoesNotChangeBorrowedFlagsAndClosedReaderIsReported) {
    int descriptors[2]{};
    ASSERT_EQ(::pipe(descriptors), 0);
    const int original = ::fcntl(descriptors[1], F_GETFL);
    dp::osal::OSALHostIo output;
    ASSERT_TRUE(output.open_output(descriptors[1]));
    EXPECT_EQ(::fcntl(descriptors[1], F_GETFL), original);
    std::array<uint8_t, 4096> page{};
    size_t written{};
    bool blocked = false;
    for (unsigned count = 0U; count < 1024U; ++count) {
        const auto result = output.try_write(page.data(), page.size(), &written);
        if (result == dp::osal::HostIoResult::kWouldBlock) { blocked = true; break; }
        ASSERT_EQ(result, dp::osal::HostIoResult::kOk);
        ASSERT_GT(written, 0U);
    }
    ASSERT_TRUE(blocked);
    ASSERT_EQ(::read(descriptors[0], page.data(), page.size()), static_cast<ssize_t>(page.size()));
    EXPECT_EQ(output.try_write(page.data(), 1U, &written), dp::osal::HostIoResult::kOk);
    EXPECT_EQ(written, 1U);
    ASSERT_EQ(::close(descriptors[0]), 0);
    EXPECT_EQ(output.try_write(page.data(), 1U, &written), dp::osal::HostIoResult::kClosed);
    EXPECT_TRUE(output.close());
    EXPECT_TRUE(output.close());
    EXPECT_EQ(output.try_write(page.data(), 1U, &written), dp::osal::HostIoResult::kClosed);
    EXPECT_EQ(::close(descriptors[1]), 0);
}

TEST(OSALHostIo, RegularOutputSharesOffsetAndExclusiveFilesPreserveExistingContent) {
    char path[] = "/tmp/dp-osal-host-io-XXXXXX";
    const int fd = ::mkstemp(path);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(::write(fd, "A", 1U), 1);
    const int flags = ::fcntl(fd, F_GETFL);
    dp::osal::OSALHostIo output;
    ASSERT_TRUE(output.open_output(fd));
    size_t count{};
    const uint8_t next = 'B';
    EXPECT_EQ(output.try_write(&next, 1U, &count), dp::osal::HostIoResult::kOk);
    EXPECT_EQ(::fcntl(fd, F_GETFL), flags);
    EXPECT_TRUE(output.close());
    char actual[2]{};
    ASSERT_EQ(::pread(fd, actual, sizeof(actual), 0), 2);
    EXPECT_EQ(actual[0], 'A'); EXPECT_EQ(actual[1], 'B');
    dp::osal::OSALHostIo file;
    EXPECT_FALSE(file.open_exclusive(path));
    EXPECT_EQ(::close(fd), 0);
    ASSERT_EQ(::unlink(path), 0);
    ASSERT_TRUE(file.open_exclusive(path));
    EXPECT_EQ(file.write_at(&next, 1U, 8U, &count), dp::osal::HostIoResult::kOk);
    uint8_t readback{};
    EXPECT_EQ(file.read_at(&readback, 1U, 8U, &count), dp::osal::HostIoResult::kOk);
    EXPECT_EQ(count, 1U); EXPECT_EQ(readback, 'B');
    EXPECT_TRUE(file.sync()); EXPECT_TRUE(file.close());
    EXPECT_EQ(::unlink(path), 0);
}

TEST(OSALHostIo, WakePreservesCreditBeforeWaitAndReportsClosedState) {
    dp::osal::OSALHostWake wake;
    EXPECT_FALSE(wake.signal());
    EXPECT_EQ(wake.wait_for_ms(0U), dp::osal::HostWakeResult::kError);
    ASSERT_TRUE(wake.open());
    EXPECT_EQ(wake.wait_for_ms(0U), dp::osal::HostWakeResult::kTimeout);
    ASSERT_TRUE(wake.signal()); ASSERT_TRUE(wake.signal());
    EXPECT_EQ(wake.wait_for_ms(0U), dp::osal::HostWakeResult::kSignaled);
    EXPECT_EQ(wake.wait_for_ms(0U), dp::osal::HostWakeResult::kSignaled);
    EXPECT_EQ(wake.wait_for_ms(0U), dp::osal::HostWakeResult::kTimeout);
    EXPECT_TRUE(wake.close()); EXPECT_TRUE(wake.close());
}
#endif
