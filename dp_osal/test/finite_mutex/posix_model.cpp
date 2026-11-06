/** @file posix_model.cpp @brief 实际Mutex后端的受控时钟/内核边界；检查不受NDEBUG裁掉。 */

#include <pthread.h>
#include <ctime>
#include <cerrno>
#include <cstdlib>
#define DP_OSAL_TEST_REQUIRE(value)                                                    \
    do {                                                                               \
        if (!(value)) {                                                                \
            std::fprintf(stderr, "检查失败: %s:%d: %s\n", __FILE__, __LINE__, #value); \
            std::abort();                                                              \
        }                                                                              \
    } while (false)
#include <cstdio>
#include <cstdint>
uint64_t mono = 0, release_at = 0, deadline = 0;
unsigned calls = 0, sleeps = 0, eintrs = 0;
bool clock_fail = false, sleep_fail = false;
int terminal = 0;
int test_clock(clockid_t id, timespec* out) {
    DP_OSAL_TEST_REQUIRE(id == CLOCK_MONOTONIC);
    if (clock_fail) {
        errno = EIO;
        return -1;
    }
    out->tv_sec = static_cast<time_t>(mono / 1000000000ULL);
    out->tv_nsec = static_cast<long>(mono % 1000000000ULL);
    return 0;
}
int test_try(pthread_mutex_t*) {
    ++calls;
    if (terminal) return terminal;
    return mono >= release_at ? 0 : EBUSY;
}
int test_sleep(const timespec* input, timespec*) {
    ++sleeps;
    DP_OSAL_TEST_REQUIRE(input->tv_sec >= 0 && input->tv_nsec >= 0);
    uint64_t n = static_cast<uint64_t>(input->tv_sec) * 1000000000ULL + static_cast<uint64_t>(input->tv_nsec);
    DP_OSAL_TEST_REQUIRE(n > 0 && n <= 1000000 && mono + n <= deadline);
    if (sleep_fail) {
        errno = EIO;
        return -1;
    }
    if (eintrs) {
        --eintrs;
        mono += n / 2;
        errno = EINTR;
        return -1;
    }
    mono += n;
    return 0;
}
#define DP_OSAL_DEBUG_H_
#define DP_OSAL_LOGD(...) ((void)0)
#define DP_OSAL_LOGE(...) ((void)0)
#define clock_gettime test_clock
#define pthread_mutex_trylock test_try
#define nanosleep test_sleep
#include MUTEX_BACKEND
#undef clock_gettime
#undef pthread_mutex_trylock
#undef nanosleep
void reset(uint64_t release, uint32_t ms) {
    mono = 999900000;
    deadline = mono + uint64_t(ms) * 1000000;
    release_at = mono + release;
    calls = sleeps = eintrs = 0;
    clock_fail = sleep_fail = false;
    terminal = 0;
}
int main() {
    dp::osal::Mutex m;
    reset(0, 0);
    DP_OSAL_TEST_REQUIRE(m.tryLockForMs(0));
    DP_OSAL_TEST_REQUIRE(sleeps == 0);
    reset(500000, 1);
    DP_OSAL_TEST_REQUIRE(m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(sleeps == 1);
    reset(10000000, 2);
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(2));
    DP_OSAL_TEST_REQUIRE(mono == deadline);
    DP_OSAL_TEST_REQUIRE(sleeps == 2);
    reset(600000, 1);
    eintrs = 1;
    DP_OSAL_TEST_REQUIRE(m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(sleeps == 2);
    DP_OSAL_TEST_REQUIRE(mono == deadline);
    reset(10000000, 1);
    eintrs = 2;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(mono == deadline);
    reset(10000000, 1);
    clock_fail = true;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(sleeps == 0);
    reset(10000000, 1);
    sleep_fail = true;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(sleeps == 1);
    reset(10000000, 1);
    terminal = EINVAL;
    DP_OSAL_TEST_REQUIRE(!m.tryLockForMs(1));
    DP_OSAL_TEST_REQUIRE(sleeps == 0);
    reset(500000, 1);
    DP_OSAL_TEST_REQUIRE(m.tryLockFor(1));
    DP_OSAL_TEST_REQUIRE(sleeps == 1);
    puts("POSIX 9 boundary groups PASS");
}
