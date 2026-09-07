#include "styles/style_registry.hpp"

namespace n8v::detail {
namespace {

class MaterialPaint final : public Paint {
public:
  ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const override {
    ButtonPaint paint{};
    if (style == ButtonStyle::Primary) {
      paint.background = pressed ? Color{94, 72, 150, 255} : hovered ? Color{123, 100, 184, 255} : Color{103, 80, 164, 255};
      paint.textColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{214, 202, 236, 255} : hovered ? Color{223, 211, 243, 255} : Color{232, 222, 248, 255};
      paint.textColor = {29, 25, 43, 255};
    }

    float radius = pressed ? PressedRadius : RoundRadius;
    paint.cornerRadius = {radius, radius, radius, radius};
    paint.padding = {16, 16, 10, 10};
    paint.font = FontFamily::Roboto;
    paint.fontSize = 14;
    paint.transitionSeconds = 0.2f;
    return paint;
  }

  TextPaint text(const TextOptions &options) const override {
    TextPaint paint{};
    paint.color = options.url.empty() ? options.color : Color{103, 80, 164, 255};
    paint.font = FontFamily::Roboto;
    paint.fontSize = 16;
    return paint;
  }

private:
  static constexpr float RoundRadius = 20.0f;
  static constexpr float PressedRadius = 8.0f;
};

} // namespace

const Paint &materialPaint() {
  static MaterialPaint instance;
  return instance;
}

} // namespace n8v::detail
