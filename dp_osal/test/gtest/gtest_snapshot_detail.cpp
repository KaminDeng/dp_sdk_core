
#include <gtest/gtest.h>
#include "dp_osal_system.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include "detail/dp_osal_snapshot_detail.h"
TEST(OSALSnapshotDetail, ConversionRejectsInvalidRange) {
    uint32_t out = 7U;
    EXPECT_FALSE(dp::osal::detail::ticksToUs(1U, 0U, out));
    EXPECT_EQ(out, 7U);
    ASSERT_TRUE(dp::osal::detail::ticksToUs(128U, 128U, out));
    EXPECT_EQ(out, 1000000U);
    EXPECT_FALSE(dp::osal::detail::ticksToUs(4295U, 1U, out));
    EXPECT_EQ(out, 1000000U);
    EXPECT_FALSE(dp::osal::detail::ticksToUs(UINT64_MAX - 1U, UINT64_MAX, out));
}
TEST(OSALSnapshotDetail, StatRequiresCompleteGenerationAndCheckedCounters) {
    dp::osal::detail::PosixStatSample sample{};
    EXPECT_FALSE(dp::osal::detail::parsePosixStat("1 (broken) R 0", sample));
    std::string value = "1 (name with ) right parenthesis) S";
    for (unsigned field = 4U; field <= 22U; ++field) {
        value += ' ';
        value += std::to_string(field == 14U ? 11U : field == 15U ? 7U : field == 22U ? 42U : 0U);
    }
    ASSERT_TRUE(dp::osal::detail::parsePosixStat(value, sample));
    EXPECT_EQ(sample.ticks, 18U);
    EXPECT_EQ(sample.generation, 42U);
    EXPECT_EQ(sample.state, 'S');
    const auto field_begin = value.find(" S ") + 3U;
    auto malformed = value;
    malformed.replace(field_begin, 1U, "garbage");
    EXPECT_FALSE(dp::osal::detail::parsePosixStat(malformed, sample));
    EXPECT_FALSE(dp::osal::detail::parsePosixStat("missing(" + value.substr(value.find(')') + 1U), sample));
    uint64_t total = 19U;
    EXPECT_FALSE(dp::osal::detail::parsePosixTotal("cpu 18446744073709551615 1 0 0", total));
    EXPECT_EQ(total, 19U);
}
TEST(OSALSnapshotDetail, CounterBoundRequiresIndependentWidthEpochAndFrequency) {
    using Stamp = dp::osal::detail::SnapshotCounterStamp;
    uint64_t bound = 99U;
    const Stamp start{100U, 128U, 1U, true};
    Stamp end{228U, 128U, 1U, true};
    ASSERT_TRUE(dp::osal::detail::snapshotCounterBound(start, end, 128U, bound));
    EXPECT_EQ(bound, 128U);
    end.epoch = 2U;
    EXPECT_FALSE(dp::osal::detail::snapshotCounterBound(start, end, 128U, bound));
    end.epoch = 1U;
    end.frequency = 1024U;
    EXPECT_FALSE(dp::osal::detail::snapshotCounterBound(start, end, 128U, bound));
    end.frequency = 128U;
    end.ticks = (uint64_t{1U} << 32U) + 100U;
    EXPECT_FALSE(dp::osal::detail::snapshotCounterBound(start, end, 128U, bound));
    end.ticks = 228U;
    end.valid = false;
    EXPECT_FALSE(dp::osal::detail::snapshotCounterBound(start, end, 128U, bound));
}
TEST(OSALSnapshotDetail, TryScopeRejectsNestedAndReleasesOnEarlyReturn) {
    std::atomic_flag gate = ATOMIC_FLAG_INIT;
    auto early = [&]() {
        dp::osal::detail::SnapshotTryScope outer(gate);
        EXPECT_TRUE(static_cast<bool>(outer));
        dp::osal::detail::SnapshotTryScope nested(gate);
        EXPECT_FALSE(static_cast<bool>(nested));
        return;
    };
    early();
    dp::osal::detail::SnapshotTryScope again(gate);
    EXPECT_TRUE(static_cast<bool>(again));
}
#if defined(__linux__) && DP_OSAL_TEST_PROC_WRAP
#include <dirent.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
namespace snapshot_fixture {
std::atomic<bool> enabled{false}, fail_stat{false}, fail_total{false}, malformed{false}, nested{false};
std::atomic<unsigned> generation{1U}, runtime{100U}, total{1000U}, readers{0U}, peak_readers{0U};
std::atomic<size_t> nested_count{99U};
FILE* text_file(const std::string& value) {
    FILE* file = std::tmpfile();
    if (file != nullptr) {
        (void)std::fwrite(value.data(), 1U, value.size(), file);
        std::rewind(file);
    }
    return file;
}
void reset() {
    enabled = true;
    fail_stat = false;
    fail_total = false;
    malformed = false;
    nested = false;
    generation = 1U;
    runtime = 100U;
    readers = 0U;
    peak_readers = 0U;
}
struct Scope {
    Scope() { reset(); }
    ~Scope() { enabled = false; }
};
size_t sample(dp::osal::ThreadSnapshot* rows, size_t capacity = 64U) {
    total.fetch_add(100U);
    runtime.fetch_add(10U);
    return dp::osal::System::getInstance().get_thread_snapshot(rows, capacity);
}
bool any_runtime(const dp::osal::ThreadSnapshot* rows, size_t size) {
    for (size_t i = 0U; i < size; ++i)
        if (rows[i].runtime_valid != 0U) return true;
    return false;
}
}  // namespace snapshot_fixture
extern "C" FILE* __real_fopen(const char*, const char*);
extern "C" FILE* __wrap_fopen(const char* path, const char* mode) {
    using namespace snapshot_fixture;
    if (!enabled.load()) return __real_fopen(path, mode);
    if (std::strcmp(path, "/proc/stat") == 0) {
        const unsigned concurrent = readers.fetch_add(1U) + 1U;
        unsigned peak = peak_readers.load();
        while (peak < concurrent && !peak_readers.compare_exchange_weak(peak, concurrent)) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        if (nested.exchange(false)) {
            dp::osal::ThreadSnapshot rows[2]{};
            nested_count = dp::osal::System::getInstance().get_thread_snapshot(rows, 2U);
        }
        readers.fetch_sub(1U);
        if (fail_total.load()) {
            errno = EIO;
            return nullptr;
        }
        return text_file("cpu " + std::to_string(total.load()) + " 0 0 0 0 0 0 0\n");
    }
    if (std::strstr(path, "/proc/self/task/") != nullptr && std::strlen(path) > 5U &&
        std::strcmp(path + std::strlen(path) - 5U, "/stat") == 0) {
        if (fail_stat.load()) {
            errno = EIO;
            return nullptr;
        }
        if (malformed.load()) return text_file("1 (broken) S 0\n");
        std::string value = "1 (name with ) parentheses) R";
        for (unsigned field = 4U; field <= 22U; ++field) {
            value += ' ';
            value += std::to_string(field == 14U ? runtime.load() : field == 22U ? generation.load() : 0U);
        }
        return text_file(value + "\n");
    }
    return __real_fopen(path, mode);
}
extern "C" struct dirent* __real_readdir(DIR*);
extern "C" struct dirent* __wrap_readdir(DIR* directory) { return __real_readdir(directory); }
TEST(OSALSnapshotPosix, FailedStatCannotBecomeRuntimeOrNextBaseline) {
    snapshot_fixture::Scope scope;
    dp::osal::ThreadSnapshot rows[64]{};
    ASSERT_GT(snapshot_fixture::sample(rows), 0U);
    snapshot_fixture::fail_stat = true;
    auto count = snapshot_fixture::sample(rows);
    ASSERT_GT(count, 0U);
    EXPECT_FALSE(snapshot_fixture::any_runtime(rows, count));
    snapshot_fixture::fail_stat = false;
    count = snapshot_fixture::sample(rows);
    EXPECT_FALSE(snapshot_fixture::any_runtime(rows, count));
    count = snapshot_fixture::sample(rows);
    EXPECT_TRUE(snapshot_fixture::any_runtime(rows, count));
}
TEST(OSALSnapshotPosix, ParseFailureAndGenerationReuseCannotBecomeRuntime) {
    snapshot_fixture::Scope scope;
    dp::osal::ThreadSnapshot rows[64]{};
    (void)snapshot_fixture::sample(rows);
    (void)snapshot_fixture::sample(rows);
    snapshot_fixture::malformed = true;
    auto count = snapshot_fixture::sample(rows);
    EXPECT_FALSE(snapshot_fixture::any_runtime(rows, count));
    snapshot_fixture::malformed = false;
    (void)snapshot_fixture::sample(rows);
    snapshot_fixture::generation = 2U;
    count = snapshot_fixture::sample(rows);
    EXPECT_FALSE(snapshot_fixture::any_runtime(rows, count));
}
TEST(OSALSnapshotPosix, TotalReadFailureDoesNotPublishBaseline) {
    snapshot_fixture::Scope scope;
    dp::osal::ThreadSnapshot rows[64]{};
    (void)snapshot_fixture::sample(rows);
    (void)snapshot_fixture::sample(rows);
    snapshot_fixture::fail_total = true;
    auto count = snapshot_fixture::sample(rows);
    EXPECT_FALSE(snapshot_fixture::any_runtime(rows, count));
    snapshot_fixture::fail_total = false;
    count = snapshot_fixture::sample(rows);
    EXPECT_FALSE(snapshot_fixture::any_runtime(rows, count));
    count = snapshot_fixture::sample(rows);
    EXPECT_TRUE(snapshot_fixture::any_runtime(rows, count));
}
TEST(OSALSnapshotPosix, TruncatedWindowDoesNotPublishBaseline) {
    snapshot_fixture::Scope scope;
    dp::osal::ThreadSnapshot rows[64]{};
    std::atomic<bool> stop{false};
    std::thread worker([&] {
        while (!stop.load()) std::this_thread::yield();
    });
    (void)snapshot_fixture::sample(rows);
    auto count = snapshot_fixture::sample(rows, 1U);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(rows[0].validity, static_cast<uint8_t>(dp::osal::MeasurementValidity::kSampleTruncated));
    EXPECT_EQ(rows[0].runtime_valid, 0U);
    count = snapshot_fixture::sample(rows);
    EXPECT_FALSE(snapshot_fixture::any_runtime(rows, count));
    stop = true;
    worker.join();
}
TEST(OSALSnapshotPosix, ConcurrentGettersSerializeTotalReadAndPublication) {
    snapshot_fixture::Scope scope;
    std::atomic<unsigned> ready{0U};
    std::vector<std::thread> workers;
    for (unsigned i = 0U; i < 4U; ++i)
        workers.emplace_back([&] {
            ready.fetch_add(1U);
            while (ready.load() != 4U) std::this_thread::yield();
            dp::osal::ThreadSnapshot rows[64]{};
            (void)snapshot_fixture::sample(rows);
        });
    for (auto& worker : workers) worker.join();
    EXPECT_LE(snapshot_fixture::peak_readers.load(), 1U);
}
TEST(OSALSnapshotPosix, RuntimeConversionOverflowCannotBecomeValid) {
    snapshot_fixture::Scope scope;
    dp::osal::ThreadSnapshot rows[64]{};
    (void)snapshot_fixture::sample(rows);
    (void)snapshot_fixture::sample(rows);
    snapshot_fixture::runtime = UINT32_MAX - 100U;
    const auto count = snapshot_fixture::sample(rows);
    ASSERT_GT(count, 0U);
    EXPECT_FALSE(snapshot_fixture::any_runtime(rows, count));
}
TEST(OSALSnapshotPosix, RealProcGetterReturnsCheckedRuntimeWindow) {
    snapshot_fixture::enabled = false;
    dp::osal::ThreadSnapshot rows[64]{};
    auto& system = dp::osal::System::getInstance();
    ASSERT_GT(system.get_thread_snapshot(rows, 64U), 0U);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto count = system.get_thread_snapshot(rows, 64U);
    ASSERT_GT(count, 0U);
    EXPECT_GT(rows[0].counter_hz, 0U);
    EXPECT_TRUE(snapshot_fixture::any_runtime(rows, count));
}
TEST(OSALSnapshotPosix, NestedGetterReturnsNoSampleWithoutOverwritingHistory) {
    snapshot_fixture::Scope scope;
    dp::osal::ThreadSnapshot rows[64]{};
    snapshot_fixture::nested = true;
    ASSERT_GT(snapshot_fixture::sample(rows), 0U);
    EXPECT_EQ(snapshot_fixture::nested_count.load(), 0U);
    const auto count = snapshot_fixture::sample(rows);
    EXPECT_TRUE(snapshot_fixture::any_runtime(rows, count));
}
#endif
