#include "styles/style_registry.hpp"

namespace n8v::detail {
namespace {

class CupertinoPaint final : public Paint {
public:
  ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const override {
    ButtonPaint paint{};
    if (style == ButtonStyle::Primary) {
      paint.background = pressed ? Color{0, 98, 204, 255} : hovered ? Color{10, 132, 255, 255} : Color{0, 122, 255, 255};
      paint.textColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{216, 216, 222, 255} : hovered ? Color{229, 229, 234, 255} : Color{242, 242, 247, 255};
      paint.textColor = {0, 122, 255, 255};
    }
    paint.cornerRadius = {12, 12, 12, 12};
    paint.padding = {20, 20, 13, 13};
    paint.font = FontFamily::Inter;
    paint.fontSize = 16;
    paint.transitionSeconds = 0.12f;
    return paint;
  }

  TextPaint text(const TextOptions &options) const override {
    TextPaint paint{};
    paint.color = options.url.empty() ? options.color : Color{0, 122, 255, 255};
    paint.font = FontFamily::Inter;
    paint.fontSize = 17;
    return paint;
  }

  CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const override {
    CheckboxPaint paint{};
    if (checked) {
      paint.background = pressed ? Color{0, 98, 204, 255} : hovered ? Color{10, 132, 255, 255} : Color{0, 122, 255, 255};
      paint.textColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{216, 216, 222, 255} : hovered ? Color{229, 229, 234, 255} : Color{242, 242, 247, 255};
      paint.textColor = {0, 122, 255, 255};
    }
    paint.cornerRadius = {12, 12, 12, 12};
    paint.padding = {20, 20, 13, 13};
    paint.font = FontFamily::Inter;
    paint.fontSize = 16;
    paint.transitionSeconds = 0.12f;
    return paint;
  }

  RadioPaint radio(bool selected, bool hovered, bool pressed) const override {
    CheckboxPaint cb = checkbox(selected, hovered, pressed);
    return RadioPaint{cb.background, cb.textColor, cb.cornerRadius, cb.padding, cb.font, cb.fontSize, cb.transitionSeconds};
  }

  EntryPaint entry() const override {
    EntryPaint paint{};
    paint.background = {242, 242, 247, 255};
    paint.textColor = {0, 0, 0, 255};
    paint.placeholderColor = {150, 150, 155, 255};
    paint.cornerRadius = {10, 10, 10, 10};
    paint.padding = {14, 14, 10, 10};
    paint.font = FontFamily::Inter;
    paint.fontSize = 17;
    return paint;
  }

  DropdownPaint dropdown(bool /*open*/, bool hovered, bool /*pressed*/) const override {
    DropdownPaint paint{};
    paint.background = hovered ? Color{229, 229, 234, 255} : Color{242, 242, 247, 255};
    paint.textColor = {0, 0, 0, 255};
    paint.placeholderColor = {150, 150, 155, 255};
    paint.popupBackground = {242, 242, 247, 255};
    paint.itemHoverBackground = {229, 229, 234, 255};
    paint.cornerRadius = {10, 10, 10, 10};
    paint.padding = {14, 14, 10, 10};
    paint.font = FontFamily::Inter;
    paint.fontSize = 17;
    return paint;
  }
};

} // namespace

const Paint &cupertinoPaint() {
  static CupertinoPaint instance;
  return instance;
}

} // namespace n8v::detail
