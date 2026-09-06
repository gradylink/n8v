#include <n8v/style.hpp>

namespace n8v {
namespace {

class PlainPaint final : public Paint {
public:
  ButtonPaint button(ButtonStyle style, bool /*hovered*/) const override {
    ButtonPaint paint{};
    if (style == ButtonStyle::Primary) {
      paint.background = {40, 90, 200, 255};
      paint.hoverBackground = {60, 110, 220, 255};
      paint.textColor = {255, 255, 255, 255};
    } else {
      paint.background = {225, 225, 225, 255};
      paint.hoverBackground = {210, 210, 210, 255};
      paint.textColor = {20, 20, 20, 255};
    }
    paint.cornerRadius = {4, 4, 4, 4};
    paint.padding = {12, 12, 8, 8};
    return paint;
  }

  Color text(const TextOptions &options) const override {
    if (!options.url.empty()) return {40, 90, 200, 255}; // link color - the SDL2 renderer also underlines it
    return options.color;
  }
};

StyleFamily g_family = StyleFamily::Plain;
PlainPaint g_plainPaint;

} // namespace

const Paint &activePaint() { return g_plainPaint; }

void setStyleFamily(StyleFamily family) { g_family = family; }

StyleFamily activeStyleFamily() { return g_family; }

} // namespace n8v
