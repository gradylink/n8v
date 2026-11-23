#include "core/backend.hpp"
#include <cstdlib>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include "backends/html/html_tree_backend.hpp"
#else
#include "renderers/sdl2/sdl2_backend.hpp"
#include "sdl2_lazy_vars.h"
#endif

#ifdef N8V_HAS_GTK4_BACKEND
#include "backends/native_gtk4/gtk4_tree_backend.hpp"
#include "gtk4_lazy_vars.h"
#endif

#ifdef N8V_HAS_QT_BACKEND
#include "backends/native_qt/qt_tree_backend.hpp"
#include "qt6widgets_lazy_vars.h"
#endif

#ifdef N8V_HAS_FTXUI_BACKEND
#include "backends/native_ftxui/ftxui_tree_backend.hpp"
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <string>

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

#ifndef __EMSCRIPTEN__
enum class NativeToolkit { Gtk, Qt };

std::string lowercaseEnv(const char *name) {
  const char *value = std::getenv(name);
  std::string out = value ? value : "";
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

NativeToolkit preferredToolkitForDesktop() {
#ifdef _WIN32
  return NativeToolkit::Qt;
#else
  const std::string current = lowercaseEnv("XDG_CURRENT_DESKTOP");
  const std::string session = lowercaseEnv("DESKTOP_SESSION");

  static const char *qtDesktops[] = {"kde", "plasma", "lxqt", "lumina", "dde", "deepin"};
  for (const char *name : qtDesktops) {
    if (current.find(name) != std::string::npos || session.find(name) != std::string::npos) {
      return NativeToolkit::Qt;
    }
  }
  if (lowercaseEnv("KDE_FULL_SESSION") == "true") {
    return NativeToolkit::Qt;
  }
  return NativeToolkit::Gtk;
#endif
}

std::unique_ptr<Backend> tryGtk4Backend() {
#ifdef N8V_HAS_GTK4_BACKEND
  if (lzy_gtk4_lazy_is_available()) {
    return detail::makeGtk4TreeBackend();
  }
#endif
  return nullptr;
}

std::unique_ptr<Backend> tryQtBackend() {
#ifdef N8V_HAS_QT_BACKEND
  if (lzy_qt6widgets_lazy_is_available()) {
    return detail::makeQtTreeBackend();
  }
#endif
  return nullptr;
}
#endif

std::unique_ptr<Backend> selectBackend() {
#ifdef __EMSCRIPTEN__
  return detail::makeHtmlTreeBackend();
#else
  const char *preferredBackend = std::getenv("N8V_BACKEND");

  const auto is = [&](const char *a, const char *b) { return preferredBackend && (std::strcmp(preferredBackend, a) == 0 || std::strcmp(preferredBackend, b) == 0); };

  if (!preferredBackend) {
    const bool qtFirst = preferredToolkitForDesktop() == NativeToolkit::Qt;
    if (auto backend = qtFirst ? tryQtBackend() : tryGtk4Backend()) {
      return backend;
    }
    if (auto backend = qtFirst ? tryGtk4Backend() : tryQtBackend()) {
      return backend;
    }
  } else if (is("gtk", "gtk4")) {
    if (auto backend = tryGtk4Backend()) {
      return backend;
    }
  } else if (is("qt", "qt6")) {
    if (auto backend = tryQtBackend()) {
      return backend;
    }
  }
#ifdef N8V_HAS_FTXUI_BACKEND
  if (preferredBackend && (std::strcmp(preferredBackend, "tui") == 0 || std::strcmp(preferredBackend, "ftxui") == 0)) {
    return detail::makeFtxuiTreeBackend();
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
