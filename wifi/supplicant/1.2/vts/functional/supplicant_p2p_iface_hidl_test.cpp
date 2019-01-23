/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/logging.h>

#include <VtsHalHidlTargetTestBase.h>

#include <android/hardware/wifi/supplicant/1.2/ISupplicantP2pIface.h>

#include "supplicant_hidl_test_utils.h"
#include "supplicant_hidl_test_utils_1_2.h"

using ::android::sp;
using ::android::hardware::wifi::supplicant::V1_0::SupplicantStatus;
using ::android::hardware::wifi::supplicant::V1_0::SupplicantStatusCode;
using ::android::hardware::wifi::supplicant::V1_2::ISupplicantP2pIface;

namespace {
constexpr uint8_t kTestSsid[] = {'D', 'I', 'R', 'E', 'C', 'T', '-', 'x',
                                 'y', '-', 'H', 'E', 'L', 'L', 'O'};
constexpr char kTestPassphrase[] = "P2pWorld1234";
constexpr uint8_t kTestZeroMacAddr[] = {[0 ... 5] = 0x0};
}  // namespace

class SupplicantP2pIfaceHidlTest : public ::testing::VtsHalHidlTargetTestBase {
   public:
    virtual void SetUp() override {
        startSupplicantAndWaitForHidlService();
        EXPECT_TRUE(turnOnExcessiveLogging());
        p2p_iface_ = getSupplicantP2pIface_1_2();
        ASSERT_NE(p2p_iface_.get(), nullptr);
    }

    virtual void TearDown() override { stopSupplicant(); }

   protected:
    // ISupplicantP2pIface object used for all tests in this fixture.
    sp<ISupplicantP2pIface> p2p_iface_;
};

/*
 * Verify that AddGroup_1_2 could create a group successfully.
 */
TEST_F(SupplicantP2pIfaceHidlTest, AddGroup_1_2_Success) {
    std::vector<uint8_t> ssid(kTestSsid, kTestSsid + sizeof(kTestSsid));
    std::string passphrase = kTestPassphrase;
    int freq = 5;
    std::array<uint8_t, 6> zero_mac_addr;
    memcpy(zero_mac_addr.data(), kTestZeroMacAddr, zero_mac_addr.size());
    bool persistent = false;
    int is_join = false;

    p2p_iface_->addGroup_1_2(ssid, passphrase, persistent, freq, zero_mac_addr,
                             is_join, [](const SupplicantStatus& status) {
                                 EXPECT_EQ(SupplicantStatusCode::SUCCESS,
                                           status.code);
                             });
}

/*
 * Verify that AddGroup_1_2 fails due to invalid SSID.
 */
TEST_F(SupplicantP2pIfaceHidlTest, AddGroup_1_2_FailureInvalidSsid) {
    std::vector<uint8_t> ssid;
    std::string passphrase = kTestPassphrase;
    int freq = 5;
    std::array<uint8_t, 6> zero_mac_addr;
    memcpy(zero_mac_addr.data(), kTestZeroMacAddr, zero_mac_addr.size());
    bool persistent = false;
    int is_join = false;

    p2p_iface_->addGroup_1_2(
        ssid, passphrase, persistent, freq, zero_mac_addr, is_join,
        [](const SupplicantStatus& status) {
            EXPECT_EQ(SupplicantStatusCode::FAILURE_ARGS_INVALID, status.code);
        });
}

/*
 * Verify that AddGroup_1_2 fails due to invalid passphrase.
 */
TEST_F(SupplicantP2pIfaceHidlTest, AddGroup_1_2_FailureInvalidPassphrase) {
    std::vector<uint8_t> ssid(kTestSsid, kTestSsid + sizeof(kTestSsid));
    std::string passphrase = "1234";
    int freq = 5;
    std::array<uint8_t, 6> zero_mac_addr;
    memcpy(zero_mac_addr.data(), kTestZeroMacAddr, zero_mac_addr.size());
    bool persistent = false;
    int is_join = false;

    p2p_iface_->addGroup_1_2(
        ssid, passphrase, persistent, freq, zero_mac_addr, is_join,
        [](const SupplicantStatus& status) {
            EXPECT_EQ(SupplicantStatusCode::FAILURE_ARGS_INVALID, status.code);
        });
}

/*
 * Verify that AddGroup_1_2 fails due to invalid frequency.
 */
TEST_F(SupplicantP2pIfaceHidlTest, AddGroup_1_2_FailureInvalidFrequency) {
    std::vector<uint8_t> ssid(kTestSsid, kTestSsid + sizeof(kTestSsid));
    std::string passphrase = kTestPassphrase;
    int freq = 9999;
    std::array<uint8_t, 6> zero_mac_addr;
    memcpy(zero_mac_addr.data(), kTestZeroMacAddr, zero_mac_addr.size());
    bool persistent = false;
    int is_join = false;

    p2p_iface_->addGroup_1_2(
        ssid, passphrase, persistent, freq, zero_mac_addr, is_join,
        [](const SupplicantStatus& status) {
            EXPECT_EQ(SupplicantStatusCode::FAILURE_UNKNOWN, status.code);
        });
}
