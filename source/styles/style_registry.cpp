#include "style_registry.hpp"

#include <cstdlib>
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

} // namespace

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
}

StyleFamily activeStyleFamily() { return currentFamily; }

namespace detail {

void suppressEnvStyleDefault() {
  if (!currentFamilyExplicit) currentFamily = StyleFamily::Plain;
}

} // namespace detail

} // namespace n8v
