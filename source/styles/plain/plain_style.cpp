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
      paint.checkColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{195, 195, 195, 255} : hovered ? Color{210, 210, 210, 255} : Color{225, 225, 225, 255};
    }
    paint.cornerRadius = {3, 3, 3, 3};
    paint.indicatorSize = 18.0f;
    paint.padding = {12, 12, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  RadioPaint radio(bool selected, bool hovered, bool pressed) const override {
    RadioPaint paint{};
    if (selected) {
      paint.background = pressed ? Color{30, 72, 160, 255} : hovered ? Color{60, 110, 220, 255} : Color{40, 90, 200, 255};
      paint.dotColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{195, 195, 195, 255} : hovered ? Color{210, 210, 210, 255} : Color{225, 225, 225, 255};
    }
    paint.indicatorSize = 18.0f;
    paint.padding = {12, 12, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  TogglePaint toggle(bool on, bool /*hovered*/, bool /*pressed*/) const override {
    TogglePaint paint{};
    paint.trackOnColor = {40, 90, 200, 255};
    paint.trackOffColor = {210, 210, 210, 255};
    paint.knobOnColor = {255, 255, 255, 255};
    paint.knobOffColor = {255, 255, 255, 255};
    paint.trackWidth = 44.0f;
    paint.trackHeight = 24.0f;
    paint.knobSizeOff = 18.0f;
    paint.knobSizeOn = 18.0f;
    paint.padding = {12, 12, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    paint.transitionSeconds = 0.15f;
    (void)on;
    return paint;
  }

  EntryPaint entry(bool /*focused*/, bool /*hasValue*/) const override {
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

  DropdownPaint dropdown(bool /*open*/, bool /*hasSelection*/, bool hovered, bool /*pressed*/) const override {
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

  SliderPaint slider(bool /*hovered*/, bool /*pressed*/) const override {
    SliderPaint paint{};
    paint.trackColor = {225, 225, 225, 255};
    paint.fillColor = {40, 90, 200, 255};
    paint.thumbColor = {30, 72, 160, 255};
    paint.trackHeight = 6.0f;
    paint.thumbWidth = 18.0f;
    paint.thumbHeight = 18.0f;
    return paint;
  }

  ImagePaint image() const override {
    ImagePaint paint{};
    paint.cornerRadius = {3, 3, 3, 3};
    return paint;
  }

  IconPaint icon() const override {
    IconPaint paint{};
    paint.tint = {20, 20, 20, 255};
    paint.defaultSize = 20.0f;
    return paint;
  }
};

} // namespace

const Paint &plainPaint() {
  static PlainPaint instance;
  return instance;
}

} // namespace n8v::detail
