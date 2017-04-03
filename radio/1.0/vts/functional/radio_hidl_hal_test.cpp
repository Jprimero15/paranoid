/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <radio_hidl_hal_utils.h>

void RadioHidlTest::SetUp() {
  radio = ::testing::VtsHalHidlTargetTestBase::getService<IRadio>(
      hidl_string("rild"));
  ASSERT_NE(radio, nullptr);

  radioRsp = new RadioResponse(*this);
  ASSERT_NE(radioRsp, nullptr);

  count = 0;

  radioInd = NULL;
  radio->setResponseFunctions(radioRsp, radioInd);

  radio->getIccCardStatus(1);
  EXPECT_EQ(std::cv_status::no_timeout, wait());
  EXPECT_EQ(RadioResponseType::SOLICITED, radioRsp->rspInfo.type);
  EXPECT_EQ(1, radioRsp->rspInfo.serial);
  EXPECT_EQ(RadioError::NONE, radioRsp->rspInfo.error);
}

void RadioHidlTest::TearDown() {}

void RadioHidlTest::notify() {
  std::unique_lock<std::mutex> lock(mtx);
  count++;
  cv.notify_one();
}

std::cv_status RadioHidlTest::wait() {
  std::unique_lock<std::mutex> lock(mtx);

  std::cv_status status = std::cv_status::no_timeout;
  auto now = std::chrono::system_clock::now();
  while (count == 0) {
    status = cv.wait_until(lock, now + std::chrono::seconds(TIMEOUT_PERIOD));
    if (status == std::cv_status::timeout) {
      return status;
    }
  }
  count--;
  return status;
}