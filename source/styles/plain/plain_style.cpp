#include "styles/style_registry.hpp"

namespace n8v::detail {
namespace {

class PlainPaint final : public Paint {
public:
  ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const override {
    ButtonPaint paint{};
    if (style == ButtonStyle::Primary) {
      paint.background = pressed ? Color{30, 72, 160, 255} : hovered ? Color{60, 110, 220, 255} : Color{40, 90, 200, 255};
      paint.textColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{195, 195, 195, 255} : hovered ? Color{210, 210, 210, 255} : Color{225, 225, 225, 255};
      paint.textColor = {20, 20, 20, 255};
    }
    paint.cornerRadius = {3, 3, 3, 3};
    paint.padding = {12, 12, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  TextPaint text(const TextOptions &options) const override {
    TextPaint paint{};
    paint.color = options.url.empty() ? options.color : Color{40, 90, 200, 255};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    return paint;
  }

  CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const override {
    CheckboxPaint paint{};
    if (checked) {
      paint.background = pressed ? Color{30, 72, 160, 255} : hovered ? Color{60, 110, 220, 255} : Color{40, 90, 200, 255};
      paint.textColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{195, 195, 195, 255} : hovered ? Color{210, 210, 210, 255} : Color{225, 225, 225, 255};
      paint.textColor = {20, 20, 20, 255};
    }
    paint.cornerRadius = {3, 3, 3, 3};
    paint.padding = {12, 12, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  RadioPaint radio(bool selected, bool hovered, bool pressed) const override {
    CheckboxPaint cb = checkbox(selected, hovered, pressed);
    return RadioPaint{cb.background, cb.textColor, cb.cornerRadius, cb.padding, cb.font, cb.fontSize, cb.transitionSeconds};
  }

  EntryPaint entry() const override {
    EntryPaint paint{};
    paint.background = {240, 240, 240, 255};
    paint.textColor = {20, 20, 20, 255};
    paint.placeholderColor = {140, 140, 140, 255};
    paint.cornerRadius = {3, 3, 3, 3};
    paint.padding = {10, 10, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    return paint;
  }

  DropdownPaint dropdown(bool /*open*/, bool hovered, bool /*pressed*/) const override {
    DropdownPaint paint{};
    paint.background = hovered ? Color{230, 230, 230, 255} : Color{240, 240, 240, 255};
    paint.textColor = {20, 20, 20, 255};
    paint.placeholderColor = {140, 140, 140, 255};
    paint.popupBackground = {240, 240, 240, 255};
    paint.itemHoverBackground = {225, 225, 225, 255};
    paint.cornerRadius = {3, 3, 3, 3};
    paint.padding = {10, 10, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    return paint;
  }
};

} // namespace

const Paint &plainPaint() {
  static PlainPaint instance;
  return instance;
}

} // namespace n8v::detail
