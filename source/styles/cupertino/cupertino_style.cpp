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
      paint.checkColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{216, 216, 222, 255} : hovered ? Color{229, 229, 234, 255} : Color{242, 242, 247, 255};
    }
    paint.cornerRadius = {6, 6, 6, 6};
    paint.indicatorSize = 20.0f;
    paint.padding = {20, 20, 13, 13};
    paint.font = FontFamily::Inter;
    paint.fontSize = 16;
    paint.transitionSeconds = 0.12f;
    return paint;
  }

  RadioPaint radio(bool selected, bool hovered, bool pressed) const override {
    RadioPaint paint{};
    if (selected) {
      paint.background = pressed ? Color{0, 98, 204, 255} : hovered ? Color{10, 132, 255, 255} : Color{0, 122, 255, 255};
      paint.dotColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{216, 216, 222, 255} : hovered ? Color{229, 229, 234, 255} : Color{242, 242, 247, 255};
    }
    paint.indicatorSize = 20.0f;
    paint.padding = {20, 20, 13, 13};
    paint.font = FontFamily::Inter;
    paint.fontSize = 16;
    paint.transitionSeconds = 0.12f;
    return paint;
  }

  EntryPaint entry(bool /*focused*/, bool /*hasValue*/) const override {
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

  DropdownPaint dropdown(bool /*open*/, bool /*hasSelection*/, bool hovered, bool /*pressed*/) const override {
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

  SliderPaint slider(bool /*hovered*/, bool /*pressed*/) const override {
    SliderPaint paint{};
    paint.trackColor = {229, 229, 234, 255};
    paint.fillColor = {0, 122, 255, 255};
    paint.thumbColor = {255, 255, 255, 255};
    paint.trackHeight = 2.0f;
    paint.thumbWidth = 28.0f;
    paint.thumbHeight = 28.0f;
    paint.thumbBorderColor = {0, 0, 0, 40};
    paint.thumbBorderWidth = 1.0f;
    return paint;
  }
};

} // namespace

const Paint &cupertinoPaint() {
  static CupertinoPaint instance;
  return instance;
}

} // namespace n8v::detail
