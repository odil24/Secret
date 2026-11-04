#pragma once
#include <embree4/rtcore.h>
namespace secret::acceleration {
class EmbreeDevice final {
public:
  EmbreeDevice();
  ~EmbreeDevice();
  EmbreeDevice(const EmbreeDevice&) = delete;
  EmbreeDevice& operator=(const EmbreeDevice&) = delete;
  EmbreeDevice(EmbreeDevice&&) = delete;
  EmbreeDevice& operator=(EmbreeDevice&&) = delete;
  [[nodiscard]] RTCDevice native_handle() const noexcept;
private:
  static void error_callback(void* user_data, RTCError code, const char* message) noexcept;
  RTCDevice device_{nullptr};
};
}
