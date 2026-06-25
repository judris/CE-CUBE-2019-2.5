#include <Arduino.h>
#if defined(__AVR__)
#include <stddef.h>

inline void* operator new(size_t, void* ptr) noexcept { return ptr; }
inline void operator delete(void*, void*) noexcept {}
#else
#include <new>
#endif

#include "firmware/firmware_app.hpp"

namespace {

alignas(ce_cube::FirmwareApp) uint8_t g_app_storage[sizeof(ce_cube::FirmwareApp)] =
    {0U};

ce_cube::FirmwareApp* AppInstance() {
  return reinterpret_cast<ce_cube::FirmwareApp*>(g_app_storage);
}

}  // namespace

void setup() {
  // Construct the long-lived app explicitly at startup to avoid static
  // constructor overhead on the Nano.
  new (g_app_storage) ce_cube::FirmwareApp();
  AppInstance()->Setup();
}

void loop() { AppInstance()->Loop(); }
