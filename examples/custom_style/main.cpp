#include <n8v/ui.hpp>

#undef sidebar

using namespace n8v;

namespace {

class SunsetPaint final : public Paint {
public:
  ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const override {
    ButtonPaint paint{};
    if (style == ButtonStyle::Primary) {
      paint.background = pressed ? Color{200, 70, 20, 255} : hovered ? Color{255, 130, 50, 255} : Color{255, 110, 30, 255};
      paint.textColor = {255, 250, 240, 255};
    } else {
      paint.background = pressed ? Color{70, 40, 70, 255} : hovered ? Color{100, 60, 100, 255} : Color{85, 50, 85, 255};
      paint.textColor = {255, 220, 190, 255};
    }
    paint.cornerRadius = {16, 16, 16, 16};
    paint.padding = {18, 18, 10, 10};
    paint.font = FontFamily::Inter;
    paint.fontSize = 15;
    paint.transitionSeconds = 0.1f;
    return paint;
  }

  TextPaint text(const TextOptions &options) const override {
    TextPaint paint{};
    paint.color = options.url.empty() ? Color{120, 45, 40, 255} : Color{200, 90, 20, 255};
    paint.font = FontFamily::Inter;
    paint.fontSize = 15;
    return paint;
  }

  CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const override { return plainPaint().checkbox(checked, hovered, pressed); }
  RadioPaint radio(bool selected, bool hovered, bool pressed) const override { return plainPaint().radio(selected, hovered, pressed); }
  TogglePaint toggle(bool on, bool hovered, bool pressed) const override { return plainPaint().toggle(on, hovered, pressed); }
  EntryPaint entry(bool focused, bool hasValue) const override { return plainPaint().entry(focused, hasValue); }
  DropdownPaint dropdown(bool open, bool hasSelection, bool hovered, bool pressed) const override { return plainPaint().dropdown(open, hasSelection, hovered, pressed); }
  SliderPaint slider(bool hovered, bool pressed) const override { return plainPaint().slider(hovered, pressed); }
  ImagePaint image() const override { return plainPaint().image(); }
  IconPaint icon() const override { return plainPaint().icon(); }
  SidebarPaint sidebar() const override { return plainPaint().sidebar(); }
};

} // namespace

int main() {
  if (!initialize(800, 600, "n8v custom style example")) {
    return 1;
  }

  SunsetPaint sunset;
  bool customEnabled = true;
  setCustomPaint(sunset);

  bool subscribed = false;
  int favoriteColor = 0;
  std::string name;
  float volume = 0.6f;

  while (pumpEvents()) {
    UI() {
      flex({.direction = Direction::Vertical, .gap = 16, .padding = {24, 24, 24, 24}, .width = Sizing::grow(), .height = Sizing::grow()}) {
        text({.bold = true})("Custom style example");
        text(
          "This page is styled by a custom n8v::Paint (\"Sunset\") registered with setCustomPaint(). "
          "Only button() and text() are overridden here; every other widget delegates to "
          "plainPaint(), the real Plain style."
        );

        toggle({.checked = &customEnabled, .onChange = [&](bool on) {
                  customEnabled = on;
                  if (on) {
                    setCustomPaint(sunset);
                  } else {
                    setStyleFamily(StyleFamily::Plain);
                  }
                }})("Sunset style enabled");

        button({.style = ButtonStyle::Primary})("Primary button");
        button({.style = ButtonStyle::Secondary})("Secondary button");

        checkbox({.checked = &subscribed})("Subscribe to the sunset newsletter");

        entry({.value = &name, .placeholder = "Your name"});

        flex({.direction = Direction::Vertical, .gap = 4}) {
          radio({.selected = &favoriteColor, .value = 0})("Sunrise orange");
          radio({.selected = &favoriteColor, .value = 1})("Dusk purple");
        }

        slider({.value = &volume, .min = 0.0f, .max = 1.0f});
      }
    }
  }

  shutdown();
  return 0;
}
