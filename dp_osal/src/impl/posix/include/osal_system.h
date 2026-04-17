//
// Created by kamin.deng on 2024/8/23.
//
#ifndef OSAL_SYSTEM_H_
#define OSAL_SYSTEM_H_

#include <dirent.h>
#include <unistd.h>

#include <chrono>
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
#include "osal_thread.h"  // for osal_sleep_ms_interruptible() + tl_stop_ctx

namespace osal {

class OSALSystem : public SystemBase<OSALSystem> {
    friend class SystemBase<OSALSystem>;

public:
    static OSALSystem &getInstance() {
        static OSALSystem instance;
        return instance;
    }

private:
    void doStartScheduler() {
        // POSIX 系统不需要显式启动调度器
        while (1)
            ;
    }

    void doSleepMs(const uint32_t milliseconds) const { osal_sleep_ms_interruptible(milliseconds); }

    void doSleep(const uint32_t seconds) const { std::this_thread::sleep_for(std::chrono::seconds(seconds)); }

    [[nodiscard]] uint32_t doGetTickMs() const {
        struct timespec ts {};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint32_t>(ts.tv_sec * 1000U + ts.tv_nsec / 1000000U);
    }

    void doEnterCritical() const {
        // POSIX: 使用 signal mask 模拟（非嵌套）
        // 在 POSIX 仿真环境中临界区不是真正需要的，
        // 但提供空实现保持接口一致性
    }
    void doExitCritical() const {
    }

    const char *doGetSystemInfo() const {
        // 返回一些基本的系统信息
        return "POSIX System";
    }

#if OSAL_ENABLE_THREAD_SNAPSHOT
    size_t doGetThreadSnapshot(ThreadSnapshot *buf, size_t max) const {
        if (buf == nullptr || max == 0U) {
            return 0U;
        }

#if defined(__linux__)
        auto read_total_cpu_ticks = []() -> unsigned long long {
            FILE *f = std::fopen("/proc/stat", "r");
            if (f == nullptr) {
                return 0ULL;
            }
            char line[512] = {0};
            if (std::fgets(line, sizeof(line), f) == nullptr) {
                std::fclose(f);
                return 0ULL;
            }
            std::fclose(f);

            std::istringstream iss(line);
            std::string cpu_label;
            iss >> cpu_label;
            if (cpu_label != "cpu") {
                return 0ULL;
            }
            unsigned long long v = 0ULL;
            unsigned long long total = 0ULL;
            while (iss >> v) {
                total += v;
            }
            return total;
        };

        DIR *dir = opendir("/proc/self/task");
        if (dir == nullptr) {
            return 0U;
        }

        static std::mutex s_snap_mu;
        static unsigned long long s_prev_total_ticks = 0ULL;
        static std::unordered_map<int, unsigned long long> s_prev_thread_ticks;

        const unsigned long long total_ticks_now = read_total_cpu_ticks();
        const unsigned long long total_delta = (total_ticks_now >= s_prev_total_ticks)
                                                   ? (total_ticks_now - s_prev_total_ticks)
                                                   : 0ULL;
        const long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
        const unsigned long cpu_scale = (cpu_count > 0) ? static_cast<unsigned long>(cpu_count) : 1UL;

        std::unordered_map<int, unsigned long long> thread_ticks_now;
        std::vector<int> thread_ids_order;
        size_t out = 0U;
        struct dirent *ent = nullptr;
        while ((ent = readdir(dir)) != nullptr && out < max) {
            if (ent->d_name[0] == '.') {
                continue;
            }
            int tid = std::atoi(ent->d_name);
            if (tid <= 0) {
                continue;
            }

            char comm_path[96] = {0};
            std::snprintf(comm_path, sizeof(comm_path), "/proc/self/task/%s/comm", ent->d_name);

            FILE *comm = std::fopen(comm_path, "r");
            if (comm == nullptr) {
                continue;
            }
            char name[32] = {0};
            if (std::fgets(name, sizeof(name), comm) == nullptr) {
                std::fclose(comm);
                continue;
            }
            std::fclose(comm);

            size_t len = std::strlen(name);
            if (len > 0U && name[len - 1U] == '\n') {
                name[len - 1U] = '\0';
            }

            char stat_path[96] = {0};
            std::snprintf(stat_path, sizeof(stat_path), "/proc/self/task/%s/stat", ent->d_name);
            FILE *statf = std::fopen(stat_path, "r");
            char state_ch = '?';
            unsigned long long thread_ticks = 0ULL;
            if (statf != nullptr) {
                char line[512] = {0};
                if (std::fgets(line, sizeof(line), statf) != nullptr) {
                    char *rparen = std::strrchr(line, ')');
                    if (rparen != nullptr && rparen[1] == ' ') {
                        std::istringstream iss(rparen + 2);
                        unsigned long long skip = 0ULL;
                        unsigned long long utime = 0ULL;
                        unsigned long long stime = 0ULL;
                        iss >> state_ch;
                        for (int i = 0; i < 10; ++i) {
                            iss >> skip;
                        }
                        iss >> utime >> stime;
                        thread_ticks = utime + stime;
                    }
                }
                std::fclose(statf);
            }
            thread_ticks_now[tid] = thread_ticks;
            thread_ids_order.push_back(tid);

            std::strncpy(buf[out].name, name, sizeof(buf[out].name) - 1U);
            buf[out].name[sizeof(buf[out].name) - 1U] = '\0';
            buf[out].cpu_pct_x10 = 0U;
            buf[out].stack_hwm = 0U;     // POSIX backend has no portable stack high-water metric.
            buf[out].stack_pointer = 0U;  // POSIX: 线程 SP 无可移植获取方式
            switch (state_ch) {
                case 'R':
                    buf[out].state = 1U;  // running
                    break;
                case 'S':
                case 'I':
                    buf[out].state = 2U;  // sleeping/idle
                    break;
                case 'D':
                    buf[out].state = 3U;  // blocked (uninterruptible)
                    break;
                case 'T':
                case 't':
                    buf[out].state = 4U;  // stopped/traced
                    break;
                case 'Z':
                case 'X':
                    buf[out].state = 5U;  // zombie/dead
                    break;
                default:
                    buf[out].state = 0U;  // unknown
                    break;
            }
            ++out;
        }
        closedir(dir);

        {
            std::lock_guard<std::mutex> lock(s_snap_mu);
            if (total_delta > 0ULL) {
                for (size_t i = 0; i < out && i < thread_ids_order.size(); ++i) {
                    const int tid = thread_ids_order[i];
                    auto now_it = thread_ticks_now.find(tid);
                    if (now_it == thread_ticks_now.end()) {
                        continue;
                    }
                    const unsigned long long now_ticks = now_it->second;
                    auto prev_it = s_prev_thread_ticks.find(tid);
                    if (prev_it != s_prev_thread_ticks.end() && now_ticks >= prev_it->second) {
                        const unsigned long long thread_delta = now_ticks - prev_it->second;
                        const unsigned long long x10 = (thread_delta * 1000ULL * cpu_scale) / total_delta;
                        buf[i].cpu_pct_x10 = static_cast<uint32_t>(x10 > 100000ULL ? 100000ULL : x10);
                    }
                }
            }
            s_prev_thread_ticks = std::move(thread_ticks_now);
            s_prev_total_ticks = total_ticks_now;
        }
        return out;
#else
        (void)buf;
        return 0U;
#endif
    }
#endif

    OSALSystem(){};

    ~OSALSystem(){};
};

}  // namespace osal
#endif  // OSAL_SYSTEM_H_
