#include "core/backend.hpp"
#include <cstdlib>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include "renderers/html/html_backend.hpp"
#else
#include "renderers/sdl2/sdl2_backend.hpp"
#include "sdl2_lazy_vars.h"
#endif

#ifdef N8V_HAS_GTK4_BACKEND
#include "backends/native_gtk4/gtk4_backend.hpp"
#include "gtk4_lazy_vars.h"
#endif

#ifdef N8V_HAS_QT_BACKEND
#include "backends/native_qt/qt_backend.hpp"
#include "qt6widgets_lazy_vars.h"
#endif

#ifdef N8V_HAS_MILSKO_BACKEND
#include "backends/native_milsko/milsko_backend.hpp"
#include "milsko_lazy_vars.h"
#endif

#ifdef N8V_HAS_FLTK_BACKEND
#include "backends/native_fltk/fltk_backend.hpp"
#include "fltk_lazy_vars.h"
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
#ifdef __EMSCRIPTEN__
  return detail::makeHtmlBackend();
#else
  const char *preferredBackend = std::getenv("N8V_BACKEND");

#ifdef N8V_HAS_GTK4_BACKEND
  if ((!preferredBackend || std::strcmp(preferredBackend, "gtk") == 0 || strcmp(preferredBackend, "gtk4") == 0) && lzy_gtk4_lazy_is_available()) {
    return detail::makeGtk4Backend();
  }
#endif
#ifdef N8V_HAS_QT_BACKEND
  if ((!preferredBackend || std::strcmp(preferredBackend, "qt") == 0 || std::strcmp(preferredBackend, "qt6") == 0) && lzy_qt6widgets_lazy_is_available()) {
    return detail::makeQtBackend();
  }
#endif
#ifdef N8V_HAS_MILSKO_BACKEND
  if ((!preferredBackend || std::strcmp(preferredBackend, "milsko") == 0 || std::strcmp(preferredBackend, "mw") == 0) && lzy_milsko_lazy_is_available()) {
    return detail::makeMilskoBackend();
  }
#endif
#ifdef N8V_HAS_FLTK_BACKEND
  if ((!preferredBackend || std::strcmp(preferredBackend, "fltk") == 0) && lzy_fltk_lazy_is_available()) {
    return detail::makeFltkBackend();
  }
#endif
  if (lzy_sdl2_lazy_is_available()) {
    return detail::makeSdl2Backend();
  }
  return std::make_unique<HeadlessBackend>();
#endif
}

} // namespace

Backend &activeBackend() {
  static std::unique_ptr<Backend> backend = selectBackend();
  return *backend;
}

} // namespace n8v
