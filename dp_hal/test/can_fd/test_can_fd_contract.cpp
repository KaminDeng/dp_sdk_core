#include <gtest/gtest.h>
#include "can_fd_host.h"
#include "dp_can.h"
#include <type_traits>
#include <utility>
using namespace dp::hal;
TEST(CanFdContract, SeparateLayoutDlcAndUnsupported) {
    static_assert(sizeof(CanFrame) == 16);
    static_assert(sizeof(CanFrame{}.data) == 8);
    using R = HostCanFd<1>::Reservation;
    static_assert(!std::is_copy_constructible_v<R>);
    static_assert(!std::is_move_assignable_v<R>);
    const uint8_t lengths[] = {8, 12, 16, 20, 24, 32, 48, 64};
    for (auto n : lengths) EXPECT_EQ(canFdLength(canFdDlc(n)), n);
    EXPECT_EQ(canFdDlc(9), 255);
    EXPECT_EQ(canFdLength(16), 255);
    UnsupportedCanFd u;
    EXPECT_FALSE(u.capabilities().fd);
    EXPECT_EQ(u.configure({500000, 2000000}), Status::kNotSupported);
}
TEST(CanFdContract, ReservationIdentityAndTerminalConsumption) {
    HostCanFd<1> d;
    ASSERT_EQ(d.configure({500000, 2000000}), Status::kOk);
    HostCanFd<1>::Reservation r, other;
    ASSERT_EQ(d.reserve(r), Status::kOk);
    EXPECT_EQ(d.reserve(other), Status::kBusy);
    CanFdTx tx{};
    tx.frame.dlc = 15;
    tx.frame.id = 0x123;
    tx.owner[0] = UINT64_C(0x100000001);
    ASSERT_EQ(d.submitReserved(std::move(r), tx), Status::kOk);
    EXPECT_EQ(d.reserve(r), Status::kBusy);
    uint64_t wrong[3] = {1, 0, 0};
    EXPECT_EQ(d.complete(0, d.generation(0), wrong, true), Status::kInvalidArg);
    ASSERT_EQ(d.complete(0, d.generation(0), tx.owner, true), Status::kOk);
    EXPECT_EQ(d.reserve(r), Status::kBusy);
    CanFdTerminal terminal{};
    ASSERT_EQ(d.takeTerminal(&terminal), Status::kOk);
    EXPECT_EQ(terminal.owner[0], tx.owner[0]);
    ASSERT_EQ(d.reserve(r), Status::kOk);
    EXPECT_EQ(d.complete(0, terminal.generation, tx.owner, true), Status::kInvalidArg);
    EXPECT_EQ(d.cancelReservation(r), Status::kOk);
}
TEST(CanFdContract, MultiSlotCancelSentAndBusOff) {
    HostCanFd<3> d;
    ASSERT_EQ(d.configure({500000, 4000000}), Status::kOk);
    for (uint8_t i = 0; i < 3; ++i) {
        HostCanFd<3>::Reservation r;
        ASSERT_EQ(d.reserve(r), Status::kOk);
        CanFdTx tx{};
        tx.owner[0] = i + 1;
        tx.frame.dlc = 9;
        ASSERT_EQ(d.submitReserved(std::move(r), tx), Status::kOk);
    }
    EXPECT_EQ(d.requestCancel(0), Status::kOk);
    uint64_t owner[3] = {1, 0, 0};
    EXPECT_EQ(d.complete(0, d.generation(0), owner, true), Status::kOk);
    CanFdTerminal t{};
    EXPECT_EQ(d.takeTerminal(&t), Status::kOk);
    EXPECT_TRUE(t.sent);
    d.setBusOff(true);
    HostCanFd<3>::Reservation r;
    EXPECT_EQ(d.reserve(r), Status::kError);
}
TEST(CanFdContract, ExplicitRxPoolRatesAndStickyOverflow) {
    CanFdFrame storage[1]{};
    HostCanFd<1> d(storage, 1);
    ASSERT_EQ(d.configure({500000, 4000000}), Status::kOk);
    EXPECT_EQ(d.configuration().arbitration_bitrate, 500000U);
    EXPECT_EQ(d.configuration().data_bitrate, 4000000U);
    CanFdFrame f{};
    f.id = 0x1234567;
    f.extended = true;
    f.dlc = 15;
    f.data[63] = 77;
    ASSERT_EQ(d.injectRx(f), Status::kOk);
    EXPECT_EQ(d.injectRx(f), Status::kBusy);
    EXPECT_EQ(d.diagnostics().rx_overflow_count, 1U);
    CanFdFrame out{};
    EXPECT_EQ(d.receive(&out), Status::kOk);
    EXPECT_EQ(out.data[63], 77);
    EXPECT_EQ(out.id, f.id);
    EXPECT_EQ(d.receive(&out), Status::kBusy);
    EXPECT_EQ(d.diagnostics().rx_overflow_count, 1U);
}
