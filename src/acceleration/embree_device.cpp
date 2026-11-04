#include "secret/acceleration/embree_device.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace secret::acceleration {
namespace {

[[nodiscard]] const char* error_name(const RTCError code) noexcept {
    switch (code) {
    case RTC_ERROR_NONE: return "none";
    case RTC_ERROR_UNKNOWN: return "unknown";
    case RTC_ERROR_INVALID_ARGUMENT: return "invalid argument";
    case RTC_ERROR_INVALID_OPERATION: return "invalid operation";
    case RTC_ERROR_OUT_OF_MEMORY: return "out of memory";
    case RTC_ERROR_UNSUPPORTED_CPU: return "unsupported CPU";
    case RTC_ERROR_CANCELLED: return "cancelled";
    default: return "unrecognized Embree error";
    }
}

} // namespace

EmbreeDevice::EmbreeDevice() {
    device_ = rtcNewDevice(nullptr);
    if (device_ == nullptr) {
        throw std::runtime_error("Failed to create Embree 4 device.");
    }

    rtcSetDeviceErrorFunction(device_, &EmbreeDevice::error_callback, this);
    const RTCError error = rtcGetDeviceError(device_);
    if (error != RTC_ERROR_NONE) {
        rtcReleaseDevice(device_);
        device_ = nullptr;
        throw std::runtime_error(std::string("Embree 4 initialization failed: ") + error_name(error));
    }
}

EmbreeDevice::~EmbreeDevice() noexcept {
    if (device_ != nullptr) {
        rtcReleaseDevice(device_);
    }
}

RTCDevice EmbreeDevice::native_handle() const noexcept {
    return device_;
}

void EmbreeDevice::error_callback(void*, const RTCError code, const char* message) noexcept {
    std::cerr << "[Embree] " << error_name(code);
    if (message != nullptr) {
        std::cerr << ": " << message;
    }
    std::cerr << '\n';
}

} // namespace secret::acceleration
