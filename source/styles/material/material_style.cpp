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

  CheckboxPaint checkbox(bool checked, bool /*hovered*/, bool /*pressed*/) const override {
    CheckboxPaint paint{};
    if (checked) {
      paint.background = {103, 80, 164, 255};
      paint.checkColor = {255, 255, 255, 255};
    } else {
      paint.borderColor = {73, 69, 79, 255};
      paint.borderWidth = 2.0f;
    }
    paint.cornerRadius = {2, 2, 2, 2};
    paint.indicatorSize = 18.0f;
    paint.padding = {16, 16, 10, 10};
    paint.font = FontFamily::Roboto;
    paint.fontSize = 14;
    paint.transitionSeconds = 0.15f;
    return paint;
  }

  RadioPaint radio(bool selected, bool /*hovered*/, bool /*pressed*/) const override {
    RadioPaint paint{};
    paint.borderWidth = 2.0f;
    if (selected) {
      paint.borderColor = {103, 80, 164, 255};
      paint.dotColor = {103, 80, 164, 255};
    } else {
      paint.borderColor = {73, 69, 79, 255};
    }
    paint.indicatorSize = 16.0f;
    paint.padding = {16, 16, 10, 10};
    paint.font = FontFamily::Roboto;
    paint.fontSize = 14;
    paint.transitionSeconds = 0.15f;
    return paint;
  }

  EntryPaint entry(bool focused, bool /*hasValue*/) const override {
    EntryPaint paint{};
    paint.background = {255, 255, 255, 255};
    paint.textColor = {29, 25, 43, 255};
    paint.placeholderColor = {121, 116, 126, 255};
    paint.outlined = true;
    paint.borderColor = focused ? Color{103, 80, 164, 255} : Color{121, 116, 126, 255};
    paint.borderWidth = focused ? 2.0f : 1.0f;
    paint.labelColor = focused ? Color{103, 80, 164, 255} : Color{121, 116, 126, 255};
    paint.labelFontSize = 12;
    paint.transitionSeconds = 0.15f;
    paint.cornerRadius = {4, 4, 4, 4};
    paint.padding = {12, 12, 20, 12};
    paint.font = FontFamily::Roboto;
    paint.fontSize = 16;
    return paint;
  }

  DropdownPaint dropdown(bool open, bool hasSelection, bool hovered, bool /*pressed*/) const override {
    DropdownPaint paint{};
    paint.background = hovered ? Color{223, 211, 243, 255} : Color{231, 224, 236, 255};
    paint.textColor = {29, 25, 43, 255};
    paint.placeholderColor = {121, 116, 126, 255};
    paint.popupBackground = {231, 224, 236, 255};
    paint.itemHoverBackground = {232, 222, 248, 255};
    paint.itemSelectedBackground = {223, 211, 243, 255};
    paint.cornerRadius = {4, 4, 0, 0};
    paint.padding = {16, 16, 8, 8};
    paint.font = FontFamily::Roboto;
    paint.fontSize = 16;
    paint.labelColor = (open || hasSelection) ? Color{103, 80, 164, 255} : Color{121, 116, 126, 255}; // primary / onSurfaceVariant
    paint.labelFontSize = 12;
    paint.transitionSeconds = 0.15f;
    paint.indicatorColor = open ? Color{103, 80, 164, 255} : Color{73, 69, 79, 255}; // primary / onSurfaceVariant
    paint.indicatorWidth = open ? 2.0f : 1.0f;
    return paint;
  }

  SliderPaint slider(bool /*hovered*/, bool pressed) const override {
    SliderPaint paint{};
    paint.trackColor = {232, 222, 248, 255};
    paint.fillColor = {103, 80, 164, 255};
    paint.thumbColor = {103, 80, 164, 255};
    paint.trackHeight = 16.0f;
    paint.trackGap = 6.0f;
    paint.thumbWidth = pressed ? 2.0f : 4.0f;
    paint.thumbHeight = 44.0f;
    return paint;
  }

  ImagePaint image() const override {
    ImagePaint paint{};
    paint.cornerRadius = {12, 12, 12, 12};
    return paint;
  }

  IconPaint icon() const override {
    IconPaint paint{};
    paint.tint = {28, 27, 31, 255};
    paint.defaultSize = 24.0f;
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
