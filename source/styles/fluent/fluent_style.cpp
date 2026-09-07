#include "styles/style_registry.hpp"

namespace n8v::detail {
namespace {

class FluentPaint final : public Paint {
public:
  ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const override {
    ButtonPaint paint{};
    if (style == ButtonStyle::Primary) {
      paint.background = pressed ? Color{0, 78, 143, 255} : hovered ? Color{0, 90, 168, 255} : Color{0, 103, 192, 255};
      paint.textColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{237, 237, 237, 255} : hovered ? Color{245, 245, 245, 255} : Color{251, 251, 253, 255};
      paint.textColor = {27, 27, 27, 255};
    }
    paint.cornerRadius = {4, 4, 4, 4};
    paint.padding = {14, 14, 6, 6};
    paint.font = FontFamily::Selawik;
    paint.fontSize = 14;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  TextPaint text(const TextOptions &options) const override {
    TextPaint paint{};
    paint.color = options.url.empty() ? options.color : Color{0, 103, 192, 255};
    paint.font = FontFamily::Selawik;
    paint.fontSize = 14;
    return paint;
  }
};

} // namespace

const Paint &fluentPaint() {
  static FluentPaint instance;
  return instance;
}

} // namespace n8v::detail
