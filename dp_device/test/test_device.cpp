/** @file   test_device.cpp
 *  @brief  GTest suite for the Device base class lifecycle. */

#include <gtest/gtest.h>

#include "dp_device.h"

namespace {

using dp::device::Device;
using dp::device::DeviceType;
using dp::hal::Status;

/** @brief  Test subclass that counts doOpen/doClose invocations. */
class TestDevice : public Device {
public:
    TestDevice(const char *name, DeviceType type) : Device(name, type) {}

    int openCount() const { return open_count_; }
    int closeCount() const { return close_count_; }

protected:
    Status doOpen(uint32_t /*flags*/) override {
        ++open_count_;
        return Status::kOk;
    }

    Status doClose() override {
        ++close_count_;
        return Status::kOk;
    }

private:
    int open_count_ = 0;
    int close_count_ = 0;
};

class DpDeviceTest : public ::testing::Test {
protected:
    TestDevice dev_{"test0", DeviceType::kSerial};
};

TEST_F(DpDeviceTest, InitialState) {
    EXPECT_STREQ(dev_.name(), "test0");
    EXPECT_EQ(dev_.type(), DeviceType::kSerial);
    EXPECT_FALSE(dev_.isOpen());
    EXPECT_EQ(dev_.refCount(), 0U);
}

TEST_F(DpDeviceTest, OpenCallsDoOpen) {
    EXPECT_EQ(dev_.open(0), Status::kOk);
    EXPECT_TRUE(dev_.isOpen());
    EXPECT_EQ(dev_.refCount(), 1U);
    EXPECT_EQ(dev_.openCount(), 1);
}

TEST_F(DpDeviceTest, MultipleOpenIncrementsRefCount) {
    EXPECT_EQ(dev_.open(0), Status::kOk);
    EXPECT_EQ(dev_.open(0), Status::kOk);
    EXPECT_EQ(dev_.refCount(), 2U);
    /* doOpen is called only once. */
    EXPECT_EQ(dev_.openCount(), 1);
}

TEST_F(DpDeviceTest, CloseDecrementsRefCount) {
    dev_.open(0);
    dev_.open(0);
    EXPECT_EQ(dev_.close(), Status::kOk);
    EXPECT_EQ(dev_.refCount(), 1U);
    EXPECT_TRUE(dev_.isOpen());
    EXPECT_EQ(dev_.closeCount(), 0);
}

TEST_F(DpDeviceTest, FinalCloseCallsDoClose) {
    dev_.open(0);
    EXPECT_EQ(dev_.close(), Status::kOk);
    EXPECT_FALSE(dev_.isOpen());
    EXPECT_EQ(dev_.refCount(), 0U);
    EXPECT_EQ(dev_.closeCount(), 1);
}

TEST_F(DpDeviceTest, CloseWhenNotOpenReturnsError) { EXPECT_EQ(dev_.close(), Status::kError); }

TEST_F(DpDeviceTest, DoubleCloseSecondReturnsError) {
    dev_.open(0);
    EXPECT_EQ(dev_.close(), Status::kOk);
    EXPECT_EQ(dev_.close(), Status::kError);
}

}  // namespace
