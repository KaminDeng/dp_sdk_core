/**
 * @file dp_osal_system.h
 * @brief POSIX系统服务和Linux /proc线程诊断后端。
 * @details sleep_ms使用当前Thread的协作可中断等待，sleep秒接口则直接sleep_for。
 * enter/exit_critical为空实现；StartScheduler不是启动内核，而是保留调用线程的无限忙循环。
 * Linux线程快照用/proc累计tick做相邻窗口差，不冻结线程或整个系统，可能分配容器并等待文件I/O。
 * 栈和ISR能力未提供，必须按有效标记解读；诊断不能代替设备权限或硬实时资格。
 */
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
#include "dp_osal_thread.h"  // for osal_sleep_ms_interruptible() + tl_stop_ctx

namespace dp::osal {

/**
 * @brief 提供静态单例的POSIX系统后端，CRTP基类转发到私有do*操作。
 */
class System : public SystemBase<System> {
    friend class SystemBase<System>;

public:
    /**
     * @brief 惰性创建并返回系统单例。
     * @return 静态对象引用，调用者不释放。
     */
    static System &getInstance() {
        static System instance;
        return instance;
    }

private:
    /**
     * @brief 进入无限空语句忙循环，不创建或启动POSIX调度器，也不返回。
     * @note 此循环没有sleep/yield，不应把“宿主无需启动调度器”误读为本入口为空操作。
     */
    void doStartScheduler() {
        // POSIX 系统不需要显式启动调度器
        while (1)
            ;
    }

    /**
     * @brief 使用可中断毫秒sleep辅助，runner内可观察协作停止请求。
     * @param milliseconds 相对毫秒数。
     * @note 异常启用时停止可抛内部停止异常；禁异常时仅提前返回，任务循环自行退出。
     */
    void doSleepMs(const uint32_t milliseconds) const { osal_sleep_ms_interruptible(milliseconds); }

    /**
     * @brief 直接按整秒调用std::this_thread::sleep_for。
     * @param seconds 相对秒数。
     * @note 此路径不经过停止上下文的条件变量，不能按sleep_ms语义假定会被stop立即唤醒。
     */
    void doSleep(const uint32_t seconds) const { std::this_thread::sleep_for(std::chrono::seconds(seconds)); }

    /**
     * @brief 读取CLOCK_MONOTONIC并收窄为32位毫秒。
     * @return 可回绕的系统计数，不是Unix日期。
     * @note clock_gettime返回码未检查，本接口没有独立时钟失败状态。
     */
    [[nodiscard]] uint32_t doGetTickMs() const {
        struct timespec ts {};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint32_t>(ts.tv_sec * 1000U + ts.tv_nsec / 1000000U);
    }

    /**
     * @brief 当前为空实现，不加锁、不屏蔽信号或中断、不阻止其它线程访问。
     */
    void doEnterCritical() const {
        // 当前明确无操作；没有实际signal mask或互斥，调用者不能依赖它保护共享数据。
    }
    /**
     * @brief 当前为空实现，与enter_critical配对不产生主机互斥保护。
     */
    void doExitCritical() const {
    }

    /**
     * @brief 返回固定POSIX后端说明。
     * @return 静态字符串，调用者不释放。
     */
    const char *doGetSystemInfo() const {
        // 返回一些基本的系统信息
        return "POSIX System";
    }

#if DP_OSAL_ENABLE_THREAD_SNAPSHOT
    /**
     * @brief Linux下枚举本进程任务，用相邻/proc tick差填充有界线程诊断。
     * @param[out] buf 至少容纳max项的调用者输出数组。
     * @param max 最大输出条目数，零或空缓冲直接返回0。
     * @return 实际填入项数；非Linux分支返回0，不伪造线程/栈/ISR能力。
     * @details 首次采样没有历史窗口，CPU/runtime无效。之后用系统总tick差与在线CPU数估算
     * 窗口长度，用同TID的线程tick差计算runtime和CPU×10，CPU显示最多1000。
     * 历史表为跨所有调用共享静态状态，mutex保护表更新但不冻结/proc；不同读取并非原子系统快照。
     * @note TID匹配无generation区分；stat缺失/解析失败可保留零计数，且容量耗尽没有额外截断标记。
     * 不能用返回条目数证明获得了全量稳定线程集；微秒数值收窄及有效标记也不代替来源质量检查。
     * @par 内部read_total_cpu_ticks
     * 局部lambda读/proc/stat首行cpu并累计后续数值，失败返回0；它提供采样输入，不独立校验所有proc字段。
     */
    size_t doGetThreadSnapshot(ThreadSnapshot *buf, size_t max) const {
        if (buf == nullptr || max == 0U) {
            return 0U;
        }

#if defined(__linux__)
        // 读取系统累计tick作为窗口分母；不是直接测量的单调墙钟，也没有冻结所有线程。
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

        static std::mutex s_snap_mu; ///< 保护下列共享历史表，不保证/proc读取同一时刻。
        static unsigned long long s_prev_total_ticks = 0ULL; ///< 上一次调用的系统累计tick。
        static std::unordered_map<int, unsigned long long> s_prev_thread_ticks; ///< 仅以TID为键的旧计数，未编码任务generation。
        static bool s_have_total_sample = false; ///< 是否已有总tick基线，首轮不计算有效CPU窗口。
        static uint32_t s_sample_sequence = 0U; ///< 所有调用共享的采样次序，可回绕。

        const unsigned long long total_ticks_now = read_total_cpu_ticks();
        std::lock_guard<std::mutex> lock(s_snap_mu);
        const unsigned long long total_delta = s_have_total_sample && total_ticks_now >= s_prev_total_ticks
                                                   ? (total_ticks_now - s_prev_total_ticks)
                                                   : 0ULL;
        const long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
        const unsigned long cpu_scale = (cpu_count > 0) ? static_cast<unsigned long>(cpu_count) : 1UL;
        const long clock_ticks = sysconf(_SC_CLK_TCK);
        const uint32_t counter_hz = clock_ticks > 0 ? static_cast<uint32_t>(clock_ticks) : 0U;
        // 只约束系统总tick差；没有据此完成所有线程解析/身份/换算范围的独立验证。
        const bool bounded_window = total_delta > 0ULL && total_delta <= 0x7FFFFFFFULL;
        const uint32_t sample_window_us = bounded_window && counter_hz > 0U
                                              ? static_cast<uint32_t>((total_delta * 1000000ULL) /
                                                                      (static_cast<unsigned long long>(counter_hz) *
                                                                       static_cast<unsigned long long>(cpu_scale)))
                                              : 0U;
        const uint32_t sample_sequence = ++s_sample_sequence;

        std::unordered_map<int, unsigned long long> thread_ticks_now;
        std::vector<int> thread_ids_order;
        size_t out = 0U;
        struct dirent *ent = nullptr;
        // 容量满即停止枚举；本POSIX路径未为此额外设置kSampleTruncated。
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
            // stat缺失/解析不完整仍可能保留thread_ticks=0；不要把有名字等同运行时采样严格成功。
            thread_ticks_now[tid] = thread_ticks;
            thread_ids_order.push_back(tid);

            std::strncpy(buf[out].name, name, sizeof(buf[out].name) - 1U);
            buf[out].name[sizeof(buf[out].name) - 1U] = '\0';
            buf[out].cpu_pct_x10 = 0U;
            buf[out].stack_hwm = 0U;     // POSIX backend has no portable stack high-water metric.
            buf[out].stack_pointer = 0U;  // POSIX: 线程 SP 无可移植获取方式
            buf[out].runtime_us = 0U;
            buf[out].isr_runtime_us = 0U;
            buf[out].sample_window_us = sample_window_us;
            buf[out].sample_sequence = sample_sequence;
            buf[out].counter_hz = counter_hz;
            buf[out].instrumentation_overhead_us = 0U;
            buf[out].cpu_valid = 0U;
            buf[out].runtime_valid = 0U;
            buf[out].stack_valid = 0U;
            buf[out].isr_valid = 0U;
            buf[out].isr_wrapped = 0U;
            buf[out].time_domain = static_cast<uint8_t>(MeasurementTimeDomain::kPosixProcStatTicks);
            buf[out].validity = static_cast<uint8_t>(MeasurementValidity::kInsufficientWindow);
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
            if (bounded_window) {
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
                        // 系统分母涵盖在线CPU总量，乘cpu_scale后使单线程满核约为100%，再钳到1000。
                        const unsigned long long x10 = (thread_delta * 1000ULL * cpu_scale) / total_delta;
                        buf[i].cpu_pct_x10 = static_cast<uint32_t>(x10 > 1000ULL ? 1000ULL : x10);
                        buf[i].runtime_us = counter_hz > 0U
                                                ? static_cast<uint32_t>((thread_delta * 1000000ULL) /
                                                                        static_cast<unsigned long long>(counter_hz))
                                                : 0U;
                        buf[i].cpu_valid = 1U;
                        buf[i].runtime_valid = counter_hz > 0U;
                        buf[i].validity = buf[i].runtime_valid
                                              ? static_cast<uint8_t>(MeasurementValidity::kValid)
                                              : static_cast<uint8_t>(MeasurementValidity::kUnsupported);
                    }
                }
            }
            // 只保留本次实际枚举的TID；多个客户端调用会共享并推进同一套窗口基线。
            s_prev_thread_ticks = std::move(thread_ticks_now);
            s_prev_total_ticks = total_ticks_now;
            s_have_total_sample = true;
        }
        return out;
#else
        (void)buf;
        return 0U;
#endif
    }
#endif

    /**
     * @brief 私有空构造，系统服务通过getInstance获取。
     */
    System(){};

    /**
     * @brief 私有空析构，不停止进程中的线程或设备。
     */
    ~System(){};
};

} // namespace dp::osal
#endif  // OSAL_SYSTEM_H_
