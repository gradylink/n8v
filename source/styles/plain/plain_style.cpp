#include "styles/style_registry.hpp"

namespace n8v::detail {
namespace {

struct Palette {
  Color window, text, link, surface, surfaceHover, surfacePressed, ghostHover, ghostPressed, sidebar, sidebarBorder, entry, placeholder, trackOff, card, cardBorder, listHover,
    popup;
};

const Palette &palette() {
  static const Palette light = {
    {255, 255, 255, 255},
    {20, 20, 20, 255},
    {40, 90, 200, 255},
    {225, 225, 225, 255},
    {210, 210, 210, 255},
    {195, 195, 195, 255},
    {0, 0, 0, 20},
    {0, 0, 0, 40},
    {235, 235, 235, 255},
    {210, 210, 210, 255},
    {240, 240, 240, 255},
    {140, 140, 140, 255},
    {210, 210, 210, 255},
    {250, 250, 250, 255},
    {220, 220, 220, 255},
    {0, 0, 0, 20},
    {240, 240, 240, 255},
  };
  static const Palette dark = {
    {30, 31, 42, 255},
    {225, 228, 240, 255},
    {122, 162, 255, 255},
    {48, 50, 68, 255},
    {58, 61, 82, 255},
    {40, 42, 58, 255},
    {255, 255, 255, 18},
    {255, 255, 255, 34},
    {24, 25, 35, 255},
    {46, 48, 64, 255},
    {40, 42, 58, 255},
    {128, 132, 155, 255},
    {72, 74, 96, 255},
    {37, 39, 54, 255},
    {58, 61, 80, 255},
    {255, 255, 255, 14},
    {40, 42, 58, 255},
  };
  return activeThemeIsDark() ? dark : light;
}

class PlainPaint final : public Paint {
public:
  Color windowBackground() const override { return palette().window; }

  ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const override {
    ButtonPaint paint{};
    if (style == ButtonStyle::Primary) {
      paint.background = pressed ? Color{30, 72, 160, 255} : hovered ? Color{60, 110, 220, 255} : Color{40, 90, 200, 255};
      paint.textColor = {255, 255, 255, 255};
      paint.padding = {12, 12, 7, 7};
    } else if (style == ButtonStyle::Ghost) {
      paint.background = pressed ? palette().ghostPressed : hovered ? palette().ghostHover : Color{0, 0, 0, 0};
      paint.textColor = palette().text;
      paint.padding = {7, 7, 7, 7};
    } else {
      paint.background = pressed ? palette().surfacePressed : hovered ? palette().surfaceHover : palette().surface;
      paint.textColor = palette().text;
      paint.padding = {12, 12, 7, 7};
    }
    paint.cornerRadius = {3, 3, 3, 3};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  TextPaint text(const TextOptions &options) const override {
    TextPaint paint{};
    const bool link = !options.url.empty() || options.onClick;
    const bool defaultColor =
      options.color.a < 0.0f ? false : options.color.a <= 0.0f || (options.color.r == 0.0f && options.color.g == 0.0f && options.color.b == 0.0f && options.color.a == 255.0f);
    paint.color = link ? palette().link : defaultColor ? palette().text : options.color;
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    applyTextEmphasis(paint, options);
    return paint;
  }

  CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const override {
    CheckboxPaint paint{};
    if (checked) {
      paint.background = pressed ? Color{30, 72, 160, 255} : hovered ? Color{60, 110, 220, 255} : Color{40, 90, 200, 255};
      paint.checkColor = {255, 255, 255, 255};
    } else {
      paint.background = pressed ? palette().surfacePressed : hovered ? palette().surfaceHover : palette().surface;
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
      paint.background = pressed ? palette().surfacePressed : hovered ? palette().surfaceHover : palette().surface;
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
    paint.trackOffColor = palette().trackOff;
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
    paint.background = palette().entry;
    paint.textColor = palette().text;
    paint.placeholderColor = palette().placeholder;
    paint.cornerRadius = {3, 3, 3, 3};
    paint.padding = {10, 10, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    return paint;
  }

  DropdownPaint dropdown(bool /*open*/, bool /*hasSelection*/, bool hovered, bool /*pressed*/) const override {
    DropdownPaint paint{};
    paint.background = hovered ? palette().surfaceHover : palette().entry;
    paint.textColor = palette().text;
    paint.placeholderColor = palette().placeholder;
    paint.popupBackground = palette().popup;
    paint.itemHoverBackground = palette().surface;
    paint.cornerRadius = {3, 3, 3, 3};
    paint.padding = {10, 10, 7, 7};
    paint.font = FontFamily::DejaVuSans;
    paint.fontSize = 15;
    return paint;
  }

  SliderPaint slider(bool /*hovered*/, bool /*pressed*/) const override {
    SliderPaint paint{};
    paint.trackColor = palette().surface;
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
    paint.tint = palette().text;
    paint.defaultSize = 20.0f;
    return paint;
  }

  SidebarPaint sidebar() const override {
    SidebarPaint paint{};
    paint.background = palette().sidebar;
    paint.borderColor = palette().sidebarBorder;
    paint.borderWidth = 1.0f;
    paint.padding = {10, 10, 10, 10};
    return paint;
  }

  PanelPaint panel(PanelRole role, bool hovered, bool /*pressed*/) const override {
    PanelPaint paint{};
    if (role == PanelRole::ListItem) {
      paint.background = hovered ? palette().listHover : Color{0, 0, 0, 0};
      paint.cornerRadius = {6, 6, 6, 6};
      paint.padding = {10, 10, 8, 8};
      return paint;
    }
    paint.background = palette().card;
    paint.borderColor = palette().cardBorder;
    paint.borderWidth = 1.0f;
    paint.cornerRadius = {6, 6, 6, 6};
    paint.padding = {12, 12, 12, 12};
    return paint;
  }
};

} // namespace

const Paint &plainPaint() {
  static PlainPaint instance;
  return instance;
}

} // namespace n8v::detail
