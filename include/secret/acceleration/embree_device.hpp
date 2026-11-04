#pragma once

#include <embree4/rtcore.h>

namespace secret::acceleration {

// Owns one Embree 4 device. This is an optional acceleration-SDK utility;
// the renderer continues to use Secret's native BVH unless a future backend
// explicitly consumes this device.
class EmbreeDevice final {
public:
    EmbreeDevice();
    ~EmbreeDevice() noexcept;

    EmbreeDevice(const EmbreeDevice&) = delete;
    EmbreeDevice& operator=(const EmbreeDevice&) = delete;
    EmbreeDevice(EmbreeDevice&&) = delete;
    EmbreeDevice& operator=(EmbreeDevice&&) = delete;

    [[nodiscard]] RTCDevice native_handle() const noexcept;

private:
    static void error_callback(void* user_data,
                               RTCError code,
                               const char* message) noexcept;

    RTCDevice device_ = nullptr;
};

} // namespace secret::acceleration
