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
  return fallback;
}

StyleFamily currentFamily = parseStyleFamily(std::getenv("N8V_STYLE"), StyleFamily::Plain);

} // namespace

const Paint &activePaint() {
  switch (currentFamily) {
  case StyleFamily::Material:
    return detail::materialPaint();
  case StyleFamily::Cupertino:
    return detail::cupertinoPaint();
  case StyleFamily::Fluent:
    return detail::fluentPaint();
  case StyleFamily::Plain:
  default:
    return detail::plainPaint();
  }
}

void setStyleFamily(StyleFamily family) { currentFamily = family; }

StyleFamily activeStyleFamily() { return currentFamily; }

} // namespace n8v
