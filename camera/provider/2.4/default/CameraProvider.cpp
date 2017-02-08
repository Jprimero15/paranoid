/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "CamProvider@2.4-impl"
#include <android/log.h>

#include "CameraProvider.h"
#include "CameraDevice.h"
#include <string.h>
#include <utils/Trace.h>


namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace V2_4 {
namespace implementation {

namespace {
const char *kLegacyProviderName = "legacy/0";
// "device@<version>/legacy/<id>"
const std::regex kDeviceNameRE("device@([0-9]+\\.[0-9]+)/legacy/(.+)");
const char *kHAL3_2 = "3.2";
const char *kHAL1_0 = "1.0";
const int kMaxCameraDeviceNameLen = 128;
const int kMaxCameraIdLen = 16;

} // anonymous namespace

using ::android::hardware::camera::common::V1_0::CameraMetadataType;
using ::android::hardware::camera::common::V1_0::Status;

/**
 * static callback forwarding methods from HAL to instance
 */
void CameraProvider::sCameraDeviceStatusChange(
        const struct camera_module_callbacks* callbacks,
        int camera_id,
        int new_status) {
    ALOGI("%s++", __FUNCTION__);
    sp<CameraProvider> cp = const_cast<CameraProvider*>(
            static_cast<const CameraProvider*>(callbacks));

    if (cp == nullptr) {
        ALOGE("%s: callback ops is null", __FUNCTION__);
        return;
    }

    ALOGI("%s resolved provider %p", __FUNCTION__, cp.get());

    Mutex::Autolock _l(cp->mCbLock);
    char cameraId[kMaxCameraIdLen];
    snprintf(cameraId, sizeof(cameraId), "%d", camera_id);
    std::string cameraIdStr(cameraId);
    cp->mCameraStatusMap[cameraIdStr] = (camera_device_status_t) new_status;
    if (cp->mCallbacks != nullptr) {
        CameraDeviceStatus status = (CameraDeviceStatus) new_status;
        for (auto const& deviceNamePair : cp->mCameraDeviceNames) {
            if (cameraIdStr.compare(deviceNamePair.first) == 0) {
                cp->mCallbacks->cameraDeviceStatusChange(
                        deviceNamePair.second, status);
            }
        }
    }
    ALOGI("%s--", __FUNCTION__);
}

void CameraProvider::sTorchModeStatusChange(
        const struct camera_module_callbacks* callbacks,
        const char* camera_id,
        int new_status) {
    ALOGI("%s++", __FUNCTION__);
    sp<CameraProvider> cp = const_cast<CameraProvider*>(
            static_cast<const CameraProvider*>(callbacks));

    if (cp == nullptr) {
        ALOGE("%s: callback ops is null", __FUNCTION__);
        return;
    }

    ALOGI("%s resolved provider %p", __FUNCTION__, cp.get());

    Mutex::Autolock _l(cp->mCbLock);
    if (cp->mCallbacks != nullptr) {
        std::string cameraIdStr(camera_id);
        TorchModeStatus status = (TorchModeStatus) new_status;
        for (auto const& deviceNamePair : cp->mCameraDeviceNames) {
            if (cameraIdStr.compare(deviceNamePair.first) == 0) {
                cp->mCallbacks->torchModeStatusChange(
                        deviceNamePair.second, status);
            }
        }
    }
    ALOGI("%s--", __FUNCTION__);
}

Status CameraProvider::getHidlStatus(int status) {
    switch (status) {
        case 0: return Status::OK;
        case -ENODEV: return Status::INTERNAL_ERROR;
        case -EINVAL: return Status::ILLEGAL_ARGUMENT;
        default:
            ALOGE("%s: unknown HAL status code %d", __FUNCTION__, status);
            return Status::INTERNAL_ERROR;
    }
}

bool CameraProvider::matchDeviceName(const hidl_string& deviceName, std::smatch& sm) {
    std::string deviceNameStd(deviceName.c_str());
    return std::regex_match(deviceNameStd, sm, kDeviceNameRE);
}

std::string CameraProvider::getLegacyCameraId(const hidl_string& deviceName) {
    std::smatch sm;
    bool match = matchDeviceName(deviceName, sm);
    if (!match) {
        return std::string("");
    }
    return sm[2];
}

int CameraProvider::getCameraDeviceVersion(const hidl_string& deviceName) {
    std::smatch sm;
    bool match = matchDeviceName(deviceName, sm);
    if (!match) {
        return -1;
    }
    if (sm[1].compare(kHAL3_2) == 0) {
        // maybe switched to 3.4 or define the hidl version enum later
        return CAMERA_DEVICE_API_VERSION_3_2;
    } else if (sm[1].compare(kHAL1_0) == 0) {
        return CAMERA_DEVICE_API_VERSION_1_0;
    }
    return 0;
}

std::string CameraProvider::getHidlDeviceName(
        std::string cameraId, int deviceVersion) {
    // Maybe consider create a version check method and SortedVec to speed up?
    if (deviceVersion != CAMERA_DEVICE_API_VERSION_1_0 &&
            deviceVersion != CAMERA_DEVICE_API_VERSION_3_2 &&
            deviceVersion != CAMERA_DEVICE_API_VERSION_3_3 &&
            deviceVersion != CAMERA_DEVICE_API_VERSION_3_4 ) {
        return hidl_string("");
    }
    const char* versionStr = (deviceVersion == CAMERA_DEVICE_API_VERSION_1_0) ? kHAL1_0 : kHAL3_2;
    char deviceName[kMaxCameraDeviceNameLen];
    snprintf(deviceName, sizeof(deviceName), "device@%s/legacy/%s",
            versionStr, cameraId.c_str());
    return deviceName;
}

CameraProvider::CameraProvider() :
        camera_module_callbacks_t({sCameraDeviceStatusChange,
                                   sTorchModeStatusChange}) {
    mInitFailed = initialize();
}

CameraProvider::~CameraProvider() {}

bool CameraProvider::initialize() {
    camera_module_t *rawModule;
    int err = hw_get_module(CAMERA_HARDWARE_MODULE_ID,
            (const hw_module_t **)&rawModule);
    if (err < 0) {
        ALOGE("Could not load camera HAL module: %d (%s)", err, strerror(-err));
        return true;
    }

    mModule = new CameraModule(rawModule);
    err = mModule->init();
    if (err != OK) {
        ALOGE("Could not initialize camera HAL module: %d (%s)", err, strerror(-err));
        mModule.clear();
        return true;
    }
    ALOGI("Loaded \"%s\" camera module", mModule->getModuleName());

    // Setup callback now because we are going to try openLegacy next
    err = mModule->setCallbacks(this);
    if (err != OK) {
        ALOGE("Could not set camera module callback: %d (%s)", err, strerror(-err));
        mModule.clear();
        return true;
    }

    mNumberOfLegacyCameras = mModule->getNumberOfCameras();
    for (int i = 0; i < mNumberOfLegacyCameras; i++) {
        char cameraId[kMaxCameraIdLen];
        snprintf(cameraId, sizeof(cameraId), "%d", i);
        std::string cameraIdStr(cameraId);
        mCameraStatusMap[cameraIdStr] = CAMERA_DEVICE_STATUS_PRESENT;
        mCameraIds.add(cameraIdStr);

        // initialize mCameraDeviceNames and mOpenLegacySupported
        mOpenLegacySupported[cameraIdStr] = false;
        int deviceVersion = mModule->getDeviceVersion(i);
        mCameraDeviceNames.add(
                std::make_pair(cameraIdStr,
                               getHidlDeviceName(cameraIdStr, deviceVersion)));
        if (deviceVersion >= CAMERA_DEVICE_API_VERSION_3_2 &&
                mModule->isOpenLegacyDefined()) {
            // try open_legacy to see if it actually works
            struct hw_device_t* halDev = nullptr;
            int ret = mModule->openLegacy(cameraId, CAMERA_DEVICE_API_VERSION_1_0, &halDev);
            if (ret == 0) {
                mOpenLegacySupported[cameraIdStr] = true;
                halDev->close(halDev);
                mCameraDeviceNames.add(
                        std::make_pair(cameraIdStr,
                                getHidlDeviceName(cameraIdStr, CAMERA_DEVICE_API_VERSION_1_0)));
            } else if (ret == -EBUSY || ret == -EUSERS) {
                // Looks like this provider instance is not initialized during
                // system startup and there are other camera users already.
                // Not a good sign but not fatal.
                ALOGW("%s: open_legacy try failed!", __FUNCTION__);
            }
        }
    }

    // Setup vendor tags here so HAL can setup vendor keys in camera characteristics
    VendorTagDescriptor::clearGlobalVendorTagDescriptor();
    bool setupSucceed = setUpVendorTags();
    return !setupSucceed; // return flag here is mInitFailed
}

bool CameraProvider::setUpVendorTags() {
    ATRACE_CALL();
    vendor_tag_ops_t vOps = vendor_tag_ops_t();

    // Check if vendor operations have been implemented
    if (!mModule->isVendorTagDefined()) {
        ALOGI("%s: No vendor tags defined for this device.", __FUNCTION__);
        return false;
    }

    mModule->getVendorTagOps(&vOps);

    // Ensure all vendor operations are present
    if (vOps.get_tag_count == nullptr || vOps.get_all_tags == nullptr ||
            vOps.get_section_name == nullptr || vOps.get_tag_name == nullptr ||
            vOps.get_tag_type == nullptr) {
        ALOGE("%s: Vendor tag operations not fully defined. Ignoring definitions."
               , __FUNCTION__);
        return false;
    }

    // Read all vendor tag definitions into a descriptor
    sp<VendorTagDescriptor> desc;
    status_t res;
    if ((res = VendorTagDescriptor::createDescriptorFromOps(&vOps, /*out*/desc))
            != OK) {
        ALOGE("%s: Could not generate descriptor from vendor tag operations,"
              "received error %s (%d). Camera clients will not be able to use"
              "vendor tags", __FUNCTION__, strerror(res), res);
        return false;
    }

    // Set the global descriptor to use with camera metadata
    VendorTagDescriptor::setAsGlobalVendorTagDescriptor(desc);
    const SortedVector<String8>* sectionNames = desc->getAllSectionNames();
    size_t numSections = sectionNames->size();
    std::vector<std::vector<VendorTag>> tagsBySection(numSections);
    int tagCount = desc->getTagCount();
    std::vector<uint32_t> tags(tagCount);
    desc->getTagArray(tags.data());
    for (int i = 0; i < tagCount; i++) {
        VendorTag vt;
        vt.tagId = tags[i];
        vt.tagName = desc->getTagName(tags[i]);
        vt.tagType = (CameraMetadataType) desc->getTagType(tags[i]);
        ssize_t sectionIdx = desc->getSectionIndex(tags[i]);
        tagsBySection[sectionIdx].push_back(vt);
    }
    mVendorTagSections.resize(numSections);
    for (size_t s = 0; s < numSections; s++) {
        mVendorTagSections[s].sectionName = (*sectionNames)[s].string();
        mVendorTagSections[s].tags = tagsBySection[s];
    }
    return true;
}

// Methods from ::android::hardware::camera::provider::V2_4::ICameraProvider follow.
Return<Status> CameraProvider::setCallback(const sp<ICameraProviderCallback>& callback)  {
    Mutex::Autolock _l(mCbLock);
    mCallbacks = callback;
    return Status::OK;
}

Return<void> CameraProvider::getVendorTags(getVendorTags_cb _hidl_cb)  {
    _hidl_cb(Status::OK, mVendorTagSections);
    return Void();
}

Return<void> CameraProvider::getCameraIdList(getCameraIdList_cb _hidl_cb)  {
    std::vector<hidl_string> deviceNameList;
    for (auto const& deviceNamePair : mCameraDeviceNames) {
        if (mCameraStatusMap[deviceNamePair.first] == CAMERA_DEVICE_STATUS_PRESENT) {
            deviceNameList.push_back(deviceNamePair.second);
        }
    }
    hidl_vec<hidl_string> hidlDeviceNameList(deviceNameList);
    _hidl_cb(Status::OK, hidlDeviceNameList);
    return Void();
}

Return<void> CameraProvider::isSetTorchModeSupported(isSetTorchModeSupported_cb _hidl_cb) {
    bool support = mModule->isSetTorchModeSupported();
    _hidl_cb (Status::OK, support);
    return Void();
}

Return<void> CameraProvider::getCameraDeviceInterface_V1_x(
        const hidl_string& /*cameraDeviceName*/, getCameraDeviceInterface_V1_x_cb _hidl_cb)  {
    // TODO implement after device 1.0 is implemented
    _hidl_cb(Status::INTERNAL_ERROR, nullptr);
    return Void();
}

Return<void> CameraProvider::getCameraDeviceInterface_V3_x(
        const hidl_string& cameraDeviceName, getCameraDeviceInterface_V3_x_cb _hidl_cb)  {
    std::smatch sm;
    bool match = matchDeviceName(cameraDeviceName, sm);
    if (!match) {
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    std::string cameraId = sm[2];
    std::string deviceVersion = sm[1];
    std::string deviceName(cameraDeviceName.c_str());
    ssize_t index = mCameraDeviceNames.indexOf(std::make_pair(cameraId, deviceName));
    if (index == NAME_NOT_FOUND) { // Either an illegal name or a device version mismatch
        Status status = Status::OK;
        ssize_t idx = mCameraIds.indexOf(cameraId);
        if (idx == NAME_NOT_FOUND) {
            ALOGE("%s: cannot find camera %s!", __FUNCTION__, cameraId.c_str());
            status = Status::ILLEGAL_ARGUMENT;
        } else { // invalid version
            ALOGE("%s: camera device %s does not support version %s!",
                    __FUNCTION__, cameraId.c_str(), deviceVersion.c_str());
            status = Status::OPERATION_NOT_SUPPORTED;
        }
        _hidl_cb(status, nullptr);
        return Void();
    }

    if (mCameraStatusMap.count(cameraId) == 0 ||
            mCameraStatusMap[cameraId] != CAMERA_DEVICE_STATUS_PRESENT) {
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    // TODO: we also need to keep a wp list of all generated devices to notify
    //       devices of device present status change, but then each device might
    //       need a sp<provider> to keep provider alive until all device closed?
    //       Problem: do we have external camera products to test this?
    sp<android::hardware::camera::device::V3_2::implementation::CameraDevice> device =
            new android::hardware::camera::device::V3_2::implementation::CameraDevice(
                    mModule, cameraId, mCameraDeviceNames);

    if (device == nullptr) {
        ALOGE("%s: cannot allocate camera device for id %s", __FUNCTION__, cameraId.c_str());
        _hidl_cb(Status::INTERNAL_ERROR, nullptr);
        return Void();
    }

    if (device->isInitFailed()) {
        ALOGE("%s: camera device %s init failed!", __FUNCTION__, cameraId.c_str());
        device = nullptr;
        _hidl_cb(Status::INTERNAL_ERROR, nullptr);
        return Void();
    }

    _hidl_cb (Status::OK, device);
    return Void();
}

ICameraProvider* HIDL_FETCH_ICameraProvider(const char* name) {
    if (strcmp(name, kLegacyProviderName) != 0) {
        return nullptr;
    }
    CameraProvider* provider = new CameraProvider();
    if (provider == nullptr) {
        ALOGE("%s: cannot allocate camera provider!", __FUNCTION__);
        return nullptr;
    }
    if (provider->isInitFailed()) {
        ALOGE("%s: camera provider init failed!", __FUNCTION__);
        delete provider;
        return nullptr;
    }
    return provider;
}

} // namespace implementation
}  // namespace V2_4
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android