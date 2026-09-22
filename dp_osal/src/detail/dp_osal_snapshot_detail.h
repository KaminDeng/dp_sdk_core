
#ifndef DP_OSAL_SNAPSHOT_DETAIL_H_
#define DP_OSAL_SNAPSHOT_DETAIL_H_
#include <atomic>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string_view>
namespace dp::osal::detail {

inline bool ticksToUs(uint64_t ticks, uint64_t frequency, uint32_t& out) noexcept {
    if (frequency == 0U) return false;
    constexpr uint64_t scale = 1000000U;
    const uint64_t whole = ticks / frequency, part = ticks % frequency;
    if (whole > UINT32_MAX / scale || part > UINT64_MAX / scale) return false;
    const uint64_t value = whole * scale + part * scale / frequency;
    if (value > UINT32_MAX) return false;
    out = static_cast<uint32_t>(value);
    return true;
}

class SnapshotTryScope {
public:
    explicit SnapshotTryScope(std::atomic_flag& flag) noexcept
        : flag_(flag), acquired_(!flag.test_and_set(std::memory_order_acquire)) {}
    ~SnapshotTryScope() {
        if (acquired_) flag_.clear(std::memory_order_release);
    }
    SnapshotTryScope(const SnapshotTryScope&) = delete;
    SnapshotTryScope& operator=(const SnapshotTryScope&) = delete;
    explicit operator bool() const noexcept { return acquired_; }

private:
    std::atomic_flag& flag_;
    bool acquired_;
};

inline bool snapshotUnsigned(std::string_view token, uint64_t& value) noexcept {
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    return !token.empty() && result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

inline bool snapshotToken(std::string_view& text, std::string_view& token) noexcept {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return false;
    text.remove_prefix(begin);
    const auto end = text.find_first_of(" \t\r\n");
    token = text.substr(0U, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end);
    return true;
}
struct PosixStatSample {
    uint64_t ticks{};
    uint64_t generation{};
    char state{'?'};
};

inline bool parsePosixStat(std::string_view text, PosixStatSample& output) noexcept {
    const auto open = text.find('(');
    const auto close = text.rfind(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close <= open) return false;
    std::string_view prefix = text.substr(0U, open), identity;
    uint64_t pid = 0U;
    if (!snapshotToken(prefix, identity) || !snapshotUnsigned(identity, pid) || pid == 0U ||
        prefix.find_first_not_of(" \t") != std::string_view::npos)
        return false;
    text.remove_prefix(close + 1U);
    std::string_view token;
    if (!snapshotToken(text, token) || token.size() != 1U) return false;
    PosixStatSample sample{};
    sample.state = token[0];
    uint64_t user = 0U, system = 0U;
    for (unsigned field = 4U; field <= 22U; ++field) {
        if (!snapshotToken(text, token)) return false;
        if (field == 14U && !snapshotUnsigned(token, user)) return false;
        if (field == 15U && !snapshotUnsigned(token, system)) return false;
        if (field == 22U && !snapshotUnsigned(token, sample.generation)) return false;
        if (field != 14U && field != 15U && field != 22U) {
            int64_t ignored = 0;
            const auto result = std::from_chars(token.data(), token.data() + token.size(), ignored);
            if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) return false;
        }
    }
    if (user > UINT64_MAX - system) return false;
    sample.ticks = user + system;
    output = sample;
    return true;
}

inline bool parsePosixTotal(std::string_view text, uint64_t& output) noexcept {
    std::string_view token;
    if (!snapshotToken(text, token) || token != "cpu") return false;
    uint64_t total = 0U;
    unsigned count = 0U;
    while (snapshotToken(text, token)) {
        uint64_t value = 0U;
        if (!snapshotUnsigned(token, value) || total > UINT64_MAX - value) return false;
        total += value;
        ++count;
    }
    if (count < 4U) return false;
    output = total;
    return true;
}
struct SnapshotCounterStamp {
    uint64_t ticks{};
    uint32_t frequency{};
    uint32_t epoch{};
    bool valid{};
};

inline bool snapshotCounterBound(const SnapshotCounterStamp& previous_before, const SnapshotCounterStamp& current_after,
                                 uint32_t frequency, uint64_t& bound) noexcept {
    if (!previous_before.valid || !current_after.valid || frequency == 0U || previous_before.frequency != frequency ||
        current_after.frequency != frequency || previous_before.epoch != current_after.epoch ||
        current_after.ticks < previous_before.ticks)
        return false;
    const uint64_t delta = current_after.ticks - previous_before.ticks;
    if (delta >= uint64_t{1U} << 32U) return false;
    bound = delta;
    return true;
}
}  // namespace dp::osal::detail
#endif
