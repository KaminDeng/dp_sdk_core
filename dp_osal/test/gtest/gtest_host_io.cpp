/**
 * @file gtest_host_io.cpp
 * @brief Linux/POSIX真实文件、pipe、stdin替换和计数唤醒的主机回归。
 * @details 只在POSIX且Linux时注册；会访问本进程描述符和/tmp文件，不接USB/CAN或机械臂。
 * 除输入恢复守卫外，多数资源在正常路径手动释放，不能宣称任意ASSERT早退均完整清理。
 * 源码中的测试定义不表示本轮注释工作已执行这些系统调用。
 */
#include "dp_osal.h"
#if defined(DP_OSAL_BACKEND_POSIX) && defined(__linux__)
#include "dp_osal_host_io.h"
#include "dp_osal_host_wake.h"
#include <gtest/gtest.h>
#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <memory>

/**
 * @brief 在调用者PATH_MAX缓冲中把/tmp/..规范化为/，并拒绝小缓冲、不存在目录和空参数。
 * @details 只验证当前文件系统路径解析，不证明持续目录身份绑定。
 */
TEST(OSALHostIo, CanonicalDirectoryUsesCallerStorageAndRejectsSmallBuffer) {
    using Io = dp::osal::OSALHostIo;
    const size_t capacity = Io::max_path_bytes();
    std::unique_ptr<char[]> path(new (std::nothrow) char[capacity]);
    ASSERT_NE(path, nullptr);
    EXPECT_TRUE(Io::canonical_directory("/tmp/..", path.get(), capacity));
    EXPECT_STREQ(path.get(), "/");
    EXPECT_FALSE(Io::canonical_directory("/tmp", path.get(), capacity - 1U));
    EXPECT_FALSE(Io::canonical_directory("/no/such/dpsdk-directory", path.get(), capacity));
    EXPECT_FALSE(Io::canonical_directory(nullptr, path.get(), capacity));
    EXPECT_FALSE(Io::canonical_directory("/tmp", nullptr, capacity));
}

/**
 * @brief 临时文件abcdef只读3字节abc，并检查目录及空输出被拒绝、失败used归零。
 * @details 实际执行时会创建/删除/tmp文件，不要求前缀读取返回整个文件。
 */
TEST(OSALHostIo, ReadRegularPrefixIsBoundedAndRejectsDirectories) {
    char path[]="/tmp/dpsdk-host-prefix-XXXXXX";
    const int descriptor=::mkstemp(path);
    ASSERT_GE(descriptor,0);
    ASSERT_EQ(::write(descriptor,"abcdef",6U),6);
    ASSERT_EQ(::close(descriptor),0);
    std::array<uint8_t,3U> bytes{};size_t used=9U;
    EXPECT_TRUE(dp::osal::OSALHostIo::read_regular_prefix(path,bytes.data(),bytes.size(),&used));
    EXPECT_EQ(used,3U);EXPECT_EQ(std::memcmp(bytes.data(),"abc",3U),0);
    EXPECT_FALSE(dp::osal::OSALHostIo::read_regular_prefix("/tmp",bytes.data(),bytes.size(),&used));
    EXPECT_EQ(used,0U);
    EXPECT_FALSE(dp::osal::OSALHostIo::read_regular_prefix(path,nullptr,bytes.size(),&used));
    EXPECT_EQ(::unlink(path),0);
}

/**
 * @brief 填满pipe观测背压，排空一页后恢复写入，再关闭读端验证kClosed及重复close。
 * @details 同时要求借用写端flags不变；循环最多1024页，不证明任意设备写入延迟或全部信号竞态。
 */
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

/**
 * @brief 验证普通文件共享offset追加AB，独占创建拒绝已有文件，再核新文件偏移8写读及sync。
 * @details fsync成功不等价于断电恢复演练，也不覆盖目录同步或跨文件事务。
 */
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

/**
 * @brief 核未打开错误、零超时、先发送两枚令牌后逐个消费，以及重复关闭。
 * @details 不创建等待线程，不验证竞争调度延迟；重点是计数credit与状态返回。
 */
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

/**
 * @brief 用pipe临时替换stdin，观测暂不可读、NUL字节、EOF、flags与空字节指针错误。
 * @details 局部守卫在离开作用域时恢复原stdin并关闭借用测试描述符，不触碰真实键盘设备配置。
 */
TEST(OSALHostIo, InputPollingPreservesFlagsAndReportsByteThenEof) {
    int descriptors[2]{};
    ASSERT_EQ(::pipe(descriptors), 0);
    const int previous = ::dup(STDIN_FILENO);
    ASSERT_GE(previous, 0);
    /**
     * @brief 本测试专用stdin恢复守卫，在正常返回或ASSERT早退时尝试归还描述符。
     */
    struct RestoreInput {
        int saved; ///< dup保存的原stdin，恢复后关闭。
        int read; ///< 测试pipe读端，不能与字段名read混淆成函数调用。
        int write; ///< 测试pipe写端，提前close后设-1防止重复关闭。
        /**
         * @brief 尽力dup2恢复原stdin并关闭保存/pipe描述符，忽略清理错误。
         */
        ~RestoreInput() {
            (void)::dup2(saved, STDIN_FILENO);
            (void)::close(saved); (void)::close(read);
            if (write >= 0) (void)::close(write);
        }
    } restore{previous, descriptors[0], descriptors[1]};
    ASSERT_GE(::dup2(descriptors[0], STDIN_FILENO), 0);
    const int flags = ::fcntl(descriptors[0], F_GETFL);
    dp::osal::HostInputInfo info{};
    uint8_t value = 9U;
    EXPECT_EQ(dp::osal::OSALHostIo::poll_input(&value, &info), dp::osal::HostIoResult::kWouldBlock);
    EXPECT_EQ(value, 9U); EXPECT_FALSE(info.read_attempted);
    const uint8_t sent = 0U;
    ASSERT_EQ(::write(descriptors[1], &sent, 1U), 1);
    EXPECT_EQ(dp::osal::OSALHostIo::poll_input(&value, &info), dp::osal::HostIoResult::kOk);
    EXPECT_EQ(value, 0U); EXPECT_TRUE(info.read_attempted); EXPECT_EQ(info.error, 0);
    EXPECT_EQ(::fcntl(descriptors[0], F_GETFL), flags);
    ASSERT_EQ(::close(descriptors[1]), 0); restore.write = -1;
    EXPECT_EQ(dp::osal::OSALHostIo::poll_input(&value, &info), dp::osal::HostIoResult::kClosed);
    EXPECT_TRUE(info.read_attempted);
    EXPECT_EQ(dp::osal::OSALHostIo::poll_input(nullptr, &info), dp::osal::HostIoResult::kError);
}

/**
 * @brief 检查墙钟微秒的日期下界、线程CPU时间小于墙钟且不减，并拒绝空输出/空信号回调。
 * @details 不比较两个时钟的共同起点，也不以墙钟替代单调期限；系统配置需支持线程CPU时钟。
 */
TEST(OSALHostIo, WallAndThreadCpuClocksHaveDistinctValidDomains) {
    uint64_t wall = 0U;
    uint64_t cpu = 0U;
    EXPECT_TRUE(dp::osal::OSALHostIo::wall_time_us(&wall));
    EXPECT_TRUE(dp::osal::OSALHostIo::thread_cpu_time_us(&cpu));
    EXPECT_GT(wall, UINT64_C(1700000000000000));
    EXPECT_LT(cpu, wall);
    const uint64_t previous = cpu;
    EXPECT_TRUE(dp::osal::OSALHostIo::thread_cpu_time_us(&cpu));
    EXPECT_GE(cpu, previous);
    EXPECT_FALSE(dp::osal::OSALHostIo::wall_time_us(nullptr));
    EXPECT_FALSE(dp::osal::OSALHostIo::thread_cpu_time_us(nullptr));
    EXPECT_FALSE(dp::osal::OSALHostIo::install_termination_handler(nullptr));
}
#endif
