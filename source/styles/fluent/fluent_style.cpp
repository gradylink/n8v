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
    } else if (style == ButtonStyle::Ghost) {
      paint.background = pressed ? Color{0, 0, 0, 24} : hovered ? Color{0, 0, 0, 12} : Color{0, 0, 0, 0};
      paint.textColor = {27, 27, 27, 255};
    } else {
      paint.background = pressed ? Color{237, 237, 237, 255} : hovered ? Color{245, 245, 245, 255} : Color{251, 251, 253, 255};
      paint.textColor = {27, 27, 27, 255};
    }
    paint.cornerRadius = {4, 4, 4, 4};
    paint.padding = style == ButtonStyle::Ghost ? Padding{6, 6, 6, 6} : Padding{14, 14, 6, 6};
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

  CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const override {
    CheckboxPaint paint{};
    if (checked) {
      paint.background = pressed ? Color{0, 78, 143, 255} : hovered ? Color{0, 90, 168, 255} : Color{0, 103, 192, 255};
      paint.checkColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{237, 237, 237, 255} : hovered ? Color{245, 245, 245, 255} : Color{251, 251, 253, 255};
      paint.borderColor = {96, 94, 92, 255};
      paint.borderWidth = 1.0f;
    }
    paint.cornerRadius = {4, 4, 4, 4};
    paint.indicatorSize = 18.0f;
    paint.padding = {14, 14, 6, 6};
    paint.font = FontFamily::Selawik;
    paint.fontSize = 14;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  RadioPaint radio(bool selected, bool hovered, bool pressed) const override {
    RadioPaint paint{};
    if (selected) {
      paint.background = pressed ? Color{0, 78, 143, 255} : hovered ? Color{0, 90, 168, 255} : Color{0, 103, 192, 255};
      paint.dotColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? Color{237, 237, 237, 255} : hovered ? Color{245, 245, 245, 255} : Color{251, 251, 253, 255};
      paint.borderColor = {96, 94, 92, 255};
      paint.borderWidth = 1.0f;
    }
    paint.indicatorSize = 18.0f;
    paint.padding = {14, 14, 6, 6};
    paint.font = FontFamily::Selawik;
    paint.fontSize = 14;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  TogglePaint toggle(bool /*on*/, bool /*hovered*/, bool /*pressed*/) const override {
    TogglePaint paint{};
    paint.trackOnColor = {0, 103, 192, 255};
    paint.trackOffColor = {0, 0, 0, 0};
    paint.trackBorderColor = {96, 94, 92, 255};
    paint.trackBorderWidth = 1.0f;
    paint.knobOnColor = {255, 255, 255, 255};
    paint.knobOffColor = {96, 94, 92, 255};
    paint.trackWidth = 40.0f;
    paint.trackHeight = 20.0f;
    paint.knobSizeOff = 12.0f;
    paint.knobSizeOn = 12.0f;
    paint.padding = {14, 14, 6, 6};
    paint.font = FontFamily::Selawik;
    paint.fontSize = 14;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  EntryPaint entry(bool /*focused*/, bool /*hasValue*/) const override {
    EntryPaint paint{};
    paint.background = {255, 255, 255, 255};
    paint.textColor = {27, 27, 27, 255};
    paint.placeholderColor = {150, 150, 150, 255};
    paint.cornerRadius = {4, 4, 4, 4};
    paint.padding = {10, 10, 6, 6};
    paint.font = FontFamily::Selawik;
    paint.fontSize = 14;
    return paint;
  }

  DropdownPaint dropdown(bool /*open*/, bool /*hasSelection*/, bool hovered, bool /*pressed*/) const override {
    DropdownPaint paint{};
    paint.background = hovered ? Color{245, 245, 245, 255} : Color{255, 255, 255, 255};
    paint.textColor = {27, 27, 27, 255};
    paint.placeholderColor = {150, 150, 150, 255};
    paint.popupBackground = {245, 245, 245, 255};
    paint.itemHoverBackground = {237, 237, 237, 255};
    paint.cornerRadius = {4, 4, 4, 4};
    paint.padding = {10, 10, 6, 6};
    paint.font = FontFamily::Selawik;
    paint.fontSize = 14;
    return paint;
  }

  SliderPaint slider(bool /*hovered*/, bool /*pressed*/) const override {
    SliderPaint paint{};
    paint.trackColor = {237, 237, 237, 255};
    paint.fillColor = {0, 103, 192, 255};
    paint.thumbColor = {255, 255, 255, 255};
    paint.trackHeight = 4.0f;
    paint.thumbWidth = 20.0f;
    paint.thumbHeight = 20.0f;
    paint.thumbBorderColor = {0, 103, 192, 255};
    paint.thumbBorderWidth = 4.0f;
    return paint;
  }

  ImagePaint image() const override {
    ImagePaint paint{};
    paint.cornerRadius = {4, 4, 4, 4};
    return paint;
  }

  IconPaint icon() const override {
    IconPaint paint{};
    paint.tint = {32, 31, 30, 255};
    paint.defaultSize = 20.0f;
    return paint;
  }

  SidebarPaint sidebar() const override {
    SidebarPaint paint{};
    paint.background = {243, 243, 243, 255};
    paint.borderColor = {225, 225, 225, 255};
    paint.borderWidth = 1.0f;
    paint.cornerRadius = {4, 4, 4, 4};
    paint.padding = {10, 10, 10, 10};
    return paint;
  }
};

} // namespace

const Paint &fluentPaint() {
  static FluentPaint instance;
  return instance;
}

} // namespace n8v::detail
