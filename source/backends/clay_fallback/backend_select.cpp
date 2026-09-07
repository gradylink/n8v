#include <cstdlib>
#include <cstring>
#include <n8v/backend.hpp>

#include "renderers/sdl2/sdl2_backend.hpp"
#include "sdl2_lazy_vars.h"

#ifdef N8V_HAS_GTK4_BACKEND
#include "backends/native_gtk4/gtk4_backend.hpp"
#include "gtk4_lazy_vars.h"
#endif

#include <cstdio>
#include <memory>

namespace n8v {
namespace {

class HeadlessBackend final : public Backend {
public:
  bool initialize(int, int, std::string_view) override {
    std::fprintf(stderr, "[n8v] No usable renderer backend found - running headless.\n");
    return true;
  }
  bool pumpEvents() override { return false; }
  bool pointerDown() const override { return false; }
  Clay_Dimensions windowSize() const override { return {0, 0}; }
  Clay_Dimensions measureText(std::string_view, FontFamily, uint16_t, bool, bool) const override { return {0, 0}; }
  void beginFrame() override {}
  void present(Clay_RenderCommandArray) override {}
  void setCursor(CursorKind) override {}
  void shutdown() override {}
};

std::unique_ptr<Backend> selectBackend() {
  const char *preferredBackend = std::getenv("N8V_BACKEND");

#ifdef N8V_HAS_GTK4_BACKEND
  if ((!preferredBackend || std::strcmp(preferredBackend, "gtk") == 0 || strcmp(preferredBackend, "gtk4") == 0) && lzy_gtk4_lazy_is_available()) {
    return detail::makeGtk4Backend();
  }
#endif
  if (lzy_sdl2_lazy_is_available()) {
    return detail::makeSdl2Backend();
  }
  return std::make_unique<HeadlessBackend>();
}

} // namespace

Backend &activeBackend() {
  static std::unique_ptr<Backend> backend = selectBackend();
  return *backend;
}

} // namespace n8v
