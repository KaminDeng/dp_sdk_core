
//
// Created by kamin.deng on 2024/8/23.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include <dirent.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "interface_system.h"
#include "detail/dp_osal_snapshot_detail.h"
#include "dp_osal_thread.h"  // for osal_sleep_ms_interruptible() + tl_stop_ctx

namespace dp::osal {


class System : public SystemBase<System> {
    friend class SystemBase<System>;

public:

    static System& getInstance() {
        static System instance;
        return instance;
    }

private:

    void doStartScheduler() {
        // POSIX 系统不需要显式启动调度器
        while (1);
    }


    void doSleepMs(const uint32_t milliseconds) const { osal_sleep_ms_interruptible(milliseconds); }


    void doSleep(const uint32_t seconds) const { std::this_thread::sleep_for(std::chrono::seconds(seconds)); }


    [[nodiscard]] uint32_t doGetTickMs() const {
        struct timespec ts {};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint32_t>(ts.tv_sec * 1000U + ts.tv_nsec / 1000000U);
    }


    void doEnterCritical() const {
        // 当前明确无操作；没有实际signal mask或互斥，调用者不能依赖它保护共享数据。
    }

    void doExitCritical() const {}


    const char* doGetSystemInfo() const {
        // 返回一些基本的系统信息
        return "POSIX System";
    }

#if DP_OSAL_ENABLE_THREAD_SNAPSHOT

    size_t doGetThreadSnapshot(ThreadSnapshot* buf, size_t max) const {
        if (buf == nullptr || max == 0U) return 0U;
#if defined(__linux__)
        static std::atomic_flag gate = ATOMIC_FLAG_INIT;
        detail::SnapshotTryScope scope(gate);
        if (!scope) return 0U;
        static uint64_t previous_total = 0U;
        static std::unordered_map<int, detail::PosixStatSample> previous;
        static bool have_total = false;
        static uint32_t sequence = 0U;
        auto invalidate = [&]() {
            previous.clear();
            have_total = false;
        };
        DIR* directory = opendir("/proc/self/task");
        if (directory == nullptr) {
            invalidate();
            return 0U;
        }
        uint64_t total = 0U;
        bool total_ok = false;
        FILE* file = std::fopen("/proc/stat", "r");
        if (file != nullptr) {
            char line[1024]{};
            if (std::fgets(line, sizeof(line), file) != nullptr && std::strchr(line, '\n') != nullptr)
                total_ok = detail::parsePosixTotal(line, total);
            std::fclose(file);
        }
        const long cpus = sysconf(_SC_NPROCESSORS_ONLN), ticks = sysconf(_SC_CLK_TCK);
        const uint64_t cpu_scale = cpus > 0 ? static_cast<uint64_t>(cpus) : 0U;
        const uint32_t frequency =
            ticks > 0 && static_cast<uint64_t>(ticks) <= UINT32_MAX ? static_cast<uint32_t>(ticks) : 0U;
        uint32_t window_us = 0U;
        const uint64_t delta = have_total && total_ok && total >= previous_total ? total - previous_total : 0U;
        const bool window_ok = have_total && total_ok && delta > 0U && cpu_scale > 0U && frequency > 0U &&
                               cpu_scale <= UINT64_MAX / frequency &&
                               detail::ticksToUs(delta, static_cast<uint64_t>(frequency) * cpu_scale, window_us) &&
                               window_us > 0U;
        const uint32_t sample_sequence = ++sequence;
        std::unordered_map<int, detail::PosixStatSample> current;
        size_t count = 0U;
        bool truncated = false, enumeration_failed = false;
        for (;;) {
            errno = 0;
            struct dirent* entry = readdir(directory);
            if (entry == nullptr) {
                enumeration_failed = errno != 0;
                break;
            }
            uint64_t parsed_tid = 0U;
            if (!detail::snapshotUnsigned(entry->d_name, parsed_tid) || parsed_tid == 0U || parsed_tid > INT32_MAX)
                continue;
            if (count == max) {
                truncated = true;
                break;
            }
            const int tid = static_cast<int>(parsed_tid);
            char path[96]{};
            std::snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
            char name[32]{};
            bool name_ok = false;
            file = std::fopen(path, "r");
            if (file != nullptr) {
                name_ok = std::fgets(name, sizeof(name), file) != nullptr;
                std::fclose(file);
            }
            if (!name_ok) std::strcpy(name, "unknown");
            const size_t length = std::strlen(name);
            if (length > 0U && name[length - 1U] == '\n') name[length - 1U] = '\0';
            std::snprintf(path, sizeof(path), "/proc/self/task/%d/stat", tid);
            detail::PosixStatSample sample{};
            bool stat_ok = false;
            file = std::fopen(path, "r");
            if (file != nullptr) {
                char line[4096]{};
                if (std::fgets(line, sizeof(line), file) != nullptr && std::strchr(line, '\n') != nullptr)
                    stat_ok = detail::parsePosixStat(line, sample);
                std::fclose(file);
            }
            auto& row = buf[count++];
            row = ThreadSnapshot{};
            std::strncpy(row.name, name, sizeof(row.name) - 1U);
            row.sample_sequence = sample_sequence;
            row.sample_window_us = window_us;
            row.counter_hz = frequency;
            row.time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kPosixProcStatTicks);
            row.validity = static_cast<uint8_t>(MeasurementValidity::kInsufficientWindow);
            switch (sample.state) {
                case 'R':
                    row.state = 1U;
                    break;
                case 'S':
                case 'I':
                    row.state = 2U;
                    break;
                case 'D':
                    row.state = 3U;
                    break;
                case 'T':
                case 't':
                    row.state = 4U;
                    break;
                case 'Z':
                case 'X':
                    row.state = 5U;
                    break;
                default:
                    break;
            }
            if (!stat_ok) continue;
            current[tid] = sample;
            const auto old = previous.find(tid);
            if (!window_ok || old == previous.end() || old->second.generation != sample.generation ||
                sample.ticks < old->second.ticks)
                continue;
            const uint64_t runtime_delta = sample.ticks - old->second.ticks;
            uint32_t runtime_us = 0U;
            if (!detail::ticksToUs(runtime_delta, frequency, runtime_us) ||
                runtime_delta > UINT64_MAX / 1000U / cpu_scale) {
                row.validity = static_cast<uint8_t>(MeasurementValidity::kCounterAmbiguous);
                continue;
            }
            const uint64_t percent = runtime_delta * 1000U * cpu_scale / delta;
            row.runtime_us = runtime_us;
            row.cpu_pct_x10 = static_cast<uint32_t>(percent > 1000U ? 1000U : percent);
            row.cpu_valid = 1U;
            row.runtime_valid = 1U;
            row.validity = static_cast<uint8_t>(MeasurementValidity::kValid);
        }
        closedir(directory);
        if (truncated || enumeration_failed || !total_ok || frequency == 0U || cpu_scale == 0U) {
            invalidate();
            for (size_t i = 0U; i < count; ++i) {
                buf[i].cpu_valid = 0U;
                buf[i].runtime_valid = 0U;
                buf[i].sample_window_us = 0U;
                buf[i].validity = static_cast<uint8_t>(truncated ? MeasurementValidity::kSampleTruncated
                                                                 : MeasurementValidity::kCounterAmbiguous);
            }
        } else {
            previous = std::move(current);
            previous_total = total;
            have_total = true;
        }
        return count;
#else
        return 0U;
#endif
    }

#endif


    System(){};


    ~System(){};
};

}  // namespace dp::osal
#endif  // OSAL_SYSTEM_H_
