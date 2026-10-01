/*
 * Copyright (C) 2022-2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "UdfpsHandler.xiaomi_sm6150"

#include "UdfpsHandler.h"

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <utility>

// Fingerprint hwmodule commands
#define COMMAND_NIT 10
#define PARAM_NIT_UDFPS 1
#define PARAM_NIT_NONE 0

// Touchfeature
#define TOUCH_DEV_PATH "/dev/xiaomi-touch"
#define TOUCH_UDFPS_ENABLE 10
#define TOUCH_MAGIC 0x5400
#define TOUCH_IOC_SETMODE TOUCH_MAGIC + 0
#define UDFPS_STATUS_ON 1
#define UDFPS_STATUS_OFF -1

#define FOD_UI_PATH "/sys/devices/platform/soc/soc:qcom,dsi-display/fod_ui"

using ::aidl::android::hardware::biometrics::fingerprint::AcquiredInfo;

static bool readBool(int fd, bool* value) {
    char c;
    if (lseek(fd, 0, SEEK_SET) < 0) {
        LOG(ERROR) << "failed to seek fd, err: " << errno;
        return false;
    }

    ssize_t rc = read(fd, &c, sizeof(char));
    if (rc != 1) {
        LOG(ERROR) << "failed to read bool from fd, err: " << rc;
        return false;
    }

    *value = c != '0';
    return true;
}

class XiaomiUdfpsHandler : public UdfpsHandler {
  public:
    XiaomiUdfpsHandler()
            : stop_fd_(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {}

    ~XiaomiUdfpsHandler() override {
        stop_.store(true, std::memory_order_release);
        if (stop_fd_.get() >= 0) {
            const uint64_t signal = 1;
            ssize_t rc;
            do {
                rc = write(stop_fd_.get(), &signal, sizeof(signal));
            } while (rc < 0 && errno == EINTR);
            if (rc < 0 && errno != EAGAIN) {
                LOG(ERROR) << "failed to signal FOD UI thread, err: " << errno;
            }
        }
        if (fod_ui_thread_.joinable()) {
            fod_ui_thread_.join();
        }
    }

    void init(fingerprint_device_t* device) {
        mDevice = device;
        touch_fd_.reset(open(TOUCH_DEV_PATH, O_RDWR));
        if (touch_fd_.get() < 0) {
            LOG(ERROR) << "failed to open touch device, err: " << errno;
        }

        if (stop_fd_.get() < 0) {
            LOG(ERROR) << "failed to create FOD UI stop eventfd, err: " << errno;
            return;
        }

        android::base::unique_fd fod_fd(open(FOD_UI_PATH, O_RDONLY));
        if (fod_fd.get() < 0) {
            LOG(ERROR) << "failed to open FOD UI node, err: " << errno;
            return;
        }

        fod_ui_thread_ = std::thread([this, fod_fd = std::move(fod_fd)]() mutable {
            struct pollfd poll_fds[2] = {
                    {
                            .fd = fod_fd.get(),
                            .events = POLLERR | POLLPRI,
                            .revents = 0,
                    },
                    {
                            .fd = stop_fd_.get(),
                            .events = POLLIN,
                            .revents = 0,
                    },
            };

            while (!stop_.load(std::memory_order_acquire)) {
                int rc = poll(poll_fds, 2, -1);
                if (rc < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    LOG(ERROR) << "failed to poll FOD UI node, err: " << errno;
                    break;
                }

                if (poll_fds[1].revents & POLLIN) {
                    break;
                }
                if (poll_fds[0].revents & (POLLNVAL | POLLHUP)) {
                    LOG(ERROR) << "FOD UI node became unavailable";
                    break;
                }
                if (!(poll_fds[0].revents & (POLLERR | POLLPRI))) {
                    continue;
                }

                bool udfps_enabled;
                if (!readBool(fod_fd.get(), &udfps_enabled)) {
                    break;
                }
                if (mDevice != nullptr && mDevice->extCmd != nullptr) {
                    mDevice->extCmd(mDevice, COMMAND_NIT,
                                    udfps_enabled ? PARAM_NIT_UDFPS : PARAM_NIT_NONE);
                }
            }
        });
    }

    void onFingerDown(uint32_t /*x*/, uint32_t /*y*/, float /*minor*/, float /*major*/) {
        // nothing
    }

    void onFingerUp() {
        // nothing
    }

    void onAcquired(int32_t result, int32_t vendorCode) {
        if (static_cast<AcquiredInfo>(result) == AcquiredInfo::GOOD) {
            setTouchUdfpsStatus(UDFPS_STATUS_OFF);
        } else if (vendorCode == 21 || vendorCode == 23) {
            /*
             * vendorCode = 21 waiting for fingerprint authentication
             * vendorCode = 23 waiting for fingerprint enroll
             */
            setTouchUdfpsStatus(UDFPS_STATUS_ON);
        }
    }

    void cancel() {
        setTouchUdfpsStatus(UDFPS_STATUS_OFF);
    }

  private:
    void setTouchUdfpsStatus(int status) {
        if (touch_fd_.get() < 0) {
            return;
        }

        int arg[2] = {TOUCH_UDFPS_ENABLE, status};
        if (ioctl(touch_fd_.get(), TOUCH_IOC_SETMODE, &arg) < 0) {
            LOG(ERROR) << "failed to set touch UDFPS mode, err: " << errno;
        }
    }

    fingerprint_device_t* mDevice = nullptr;
    android::base::unique_fd touch_fd_;
    android::base::unique_fd stop_fd_;
    std::atomic<bool> stop_{false};
    std::thread fod_ui_thread_;
};

static UdfpsHandler* create() {
    return new XiaomiUdfpsHandler();
}

static void destroy(UdfpsHandler* handler) {
    delete handler;
}

extern "C" UdfpsHandlerFactory UDFPS_HANDLER_FACTORY = {
        .create = create,
        .destroy = destroy,
};
