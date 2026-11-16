#include "style_registry.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>

namespace n8v {
namespace {

StyleFamily parseStyleFamily(const char *value, StyleFamily fallback) {
  if (!value) return fallback;
  std::string_view name(value);
  if (name == "plain") return StyleFamily::Plain;
  if (name == "material") return StyleFamily::Material;
  if (name == "cupertino") return StyleFamily::Cupertino;
  if (name == "fluent") return StyleFamily::Fluent;
  if (name == "custom") return StyleFamily::Custom;
  return fallback;
}

StyleFamily currentFamily = parseStyleFamily(std::getenv("N8V_STYLE"), StyleFamily::Plain);

bool currentFamilyExplicit = false;
uint64_t styleGeneration = 0;

std::string lowered(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}

std::string commandOutput(const char *command) {
  std::string out;
  if (FILE *pipe = popen(command, "r")) {
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe)) out += buffer;
    pclose(pipe);
  }
  return out;
}

bool detectDark() {
  if (const char *forced = std::getenv("N8V_THEME")) {
    const std::string value = lowered(forced);
    if (value == "dark") return true;
    if (value == "light") return false;
  }
  if (const char *gtkTheme = std::getenv("GTK_THEME")) {
    if (lowered(gtkTheme).find("dark") != std::string::npos) return true;
  }
  const std::string scheme = lowered(commandOutput("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null"));
  if (scheme.find("prefer-dark") != std::string::npos) return true;
  if (scheme.find("prefer-light") != std::string::npos || scheme.find("default") != std::string::npos) return false;
  const char *home = std::getenv("HOME");
  if (home) {
    std::ifstream kde(std::string(home) + "/.config/kdeglobals");
    for (std::string line; std::getline(kde, line);) {
      if (line.rfind("ColorScheme=", 0) == 0) return lowered(line).find("dark") != std::string::npos;
    }
    std::ifstream gtk(std::string(home) + "/.config/gtk-3.0/settings.ini");
    for (std::string line; std::getline(gtk, line);) {
      if (line.rfind("gtk-application-prefer-dark-theme", 0) == 0) return line.find('1') != std::string::npos || lowered(line).find("true") != std::string::npos;
    }
  }
  return false;
}

} // namespace

bool activeThemeIsDark() {
  static const bool dark = detectDark();
  return dark;
}

const Paint &activePaint() {
  switch (currentFamily) {
  case StyleFamily::Material:
    return detail::materialPaint();
  case StyleFamily::Cupertino:
    return detail::cupertinoPaint();
  case StyleFamily::Fluent:
    return detail::fluentPaint();
  case StyleFamily::Custom:
    return detail::customPaint();
  case StyleFamily::Plain:
  default:
    return detail::plainPaint();
  }
}

void setStyleFamily(StyleFamily family) {
  currentFamily = family;
  currentFamilyExplicit = true;
  ++styleGeneration;
}

uint64_t activeStyleGeneration() { return styleGeneration; }

StyleFamily activeStyleFamily() { return currentFamily; }

namespace detail {

void suppressEnvStyleDefault() {
  if (!currentFamilyExplicit) {
    currentFamily = StyleFamily::Plain;
    ++styleGeneration;
  }
}

} // namespace detail

} // namespace n8v
