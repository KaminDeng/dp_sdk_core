/** @file   test_device_manager.cpp
 *  @brief  GTest suite for DeviceManager registry. */

#include <gtest/gtest.h>

#include "dp_device.h"
#include "dp_device_manager.h"

namespace {

using dp::device::Device;
using dp::device::DeviceManager;
using dp::device::DeviceType;
using dp::hal::Status;

/** @brief  Minimal concrete Device for manager tests. */
class StubDevice : public Device {
public:
    StubDevice(const char *name, DeviceType type) : Device(name, type) {}
};

class DpDeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override { DeviceManager::instance().clear(); }
    void TearDown() override { DeviceManager::instance().clear(); }
};

TEST_F(DpDeviceManagerTest, RegisterAndFind) {
    StubDevice d("uart0", DeviceType::kSerial);
    EXPECT_EQ(DeviceManager::instance().registerDevice(&d), Status::kOk);
    EXPECT_EQ(DeviceManager::instance().find("uart0"), &d);
}

TEST_F(DpDeviceManagerTest, FindNotFound) { EXPECT_EQ(DeviceManager::instance().find("nonexistent"), nullptr); }

TEST_F(DpDeviceManagerTest, DuplicateNameReturnsError) {
    StubDevice d1("uart0", DeviceType::kSerial);
    StubDevice d2("uart0", DeviceType::kSerial);
    EXPECT_EQ(DeviceManager::instance().registerDevice(&d1), Status::kOk);
    EXPECT_EQ(DeviceManager::instance().registerDevice(&d2), Status::kError);
}

TEST_F(DpDeviceManagerTest, FindByTypeChecksType) {
    StubDevice d("uart0", DeviceType::kSerial);
    DeviceManager::instance().registerDevice(&d);
    EXPECT_EQ(DeviceManager::instance().findByType("uart0", DeviceType::kSerial), &d);
    EXPECT_EQ(DeviceManager::instance().findByType("uart0", DeviceType::kPin), nullptr);
}

TEST_F(DpDeviceManagerTest, UnregisterRemovesDevice) {
    StubDevice d("uart0", DeviceType::kSerial);
    DeviceManager::instance().registerDevice(&d);
    EXPECT_EQ(DeviceManager::instance().unregisterDevice("uart0"), Status::kOk);
    EXPECT_EQ(DeviceManager::instance().find("uart0"), nullptr);
}

TEST_F(DpDeviceManagerTest, UnregisterOpenDeviceReturnsBusy) {
    StubDevice d("uart0", DeviceType::kSerial);
    DeviceManager::instance().registerDevice(&d);
    d.open(0);
    EXPECT_EQ(DeviceManager::instance().unregisterDevice("uart0"), Status::kBusy);
    d.close();
}

TEST_F(DpDeviceManagerTest, ForEachEnumeratesAll) {
    StubDevice d1("a", DeviceType::kSerial);
    StubDevice d2("b", DeviceType::kPin);
    DeviceManager::instance().registerDevice(&d1);
    DeviceManager::instance().registerDevice(&d2);

    int count = 0;
    DeviceManager::instance().forEach([](Device * /*dev*/, void *ctx) { ++*static_cast<int *>(ctx); }, &count);
    EXPECT_EQ(count, 2);
}

TEST_F(DpDeviceManagerTest, FullRegistryReturnsError) {
    StubDevice devs[DeviceManager::kMaxDevices]{
        {"d00", DeviceType::kSerial}, {"d01", DeviceType::kSerial}, {"d02", DeviceType::kSerial},
        {"d03", DeviceType::kSerial}, {"d04", DeviceType::kSerial}, {"d05", DeviceType::kSerial},
        {"d06", DeviceType::kSerial}, {"d07", DeviceType::kSerial}, {"d08", DeviceType::kSerial},
        {"d09", DeviceType::kSerial}, {"d10", DeviceType::kSerial}, {"d11", DeviceType::kSerial},
        {"d12", DeviceType::kSerial}, {"d13", DeviceType::kSerial}, {"d14", DeviceType::kSerial},
        {"d15", DeviceType::kSerial}, {"d16", DeviceType::kSerial}, {"d17", DeviceType::kSerial},
        {"d18", DeviceType::kSerial}, {"d19", DeviceType::kSerial}, {"d20", DeviceType::kSerial},
        {"d21", DeviceType::kSerial}, {"d22", DeviceType::kSerial}, {"d23", DeviceType::kSerial},
        {"d24", DeviceType::kSerial}, {"d25", DeviceType::kSerial}, {"d26", DeviceType::kSerial},
        {"d27", DeviceType::kSerial}, {"d28", DeviceType::kSerial}, {"d29", DeviceType::kSerial},
        {"d30", DeviceType::kSerial}, {"d31", DeviceType::kSerial},
    };
    for (auto &d : devs) {
        EXPECT_EQ(DeviceManager::instance().registerDevice(&d), Status::kOk);
    }
    StubDevice overflow("overflow", DeviceType::kSerial);
    EXPECT_EQ(DeviceManager::instance().registerDevice(&overflow), Status::kError);
}

TEST_F(DpDeviceManagerTest, NullDeviceReturnsInvalidArg) {
    EXPECT_EQ(DeviceManager::instance().registerDevice(nullptr), Status::kInvalidArg);
}

}  // namespace
