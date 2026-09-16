#include "styles/paint_conversions.hpp"
#include "styles/style_registry.hpp"

namespace n8v::detail {
namespace {

class CustomPaint final : public Paint {
public:
  void setVTable(const n8v_custom_paint_vtable *vtable, void *userdata) {
    vtable_ = vtable ? *vtable : n8v_custom_paint_vtable{};
    userdata_ = userdata;
  }

  ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const override {
    if (vtable_.button) return fromC(vtable_.button(toC(style), hovered, pressed, userdata_));
    return plainPaint().button(style, hovered, pressed);
  }

  TextPaint text(const TextOptions &options) const override {
    if (vtable_.text) return fromC(vtable_.text(toC(options), userdata_));
    return plainPaint().text(options);
  }

  CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const override {
    if (vtable_.checkbox) return fromC(vtable_.checkbox(checked, hovered, pressed, userdata_));
    return plainPaint().checkbox(checked, hovered, pressed);
  }

  RadioPaint radio(bool selected, bool hovered, bool pressed) const override {
    if (vtable_.radio) return fromC(vtable_.radio(selected, hovered, pressed, userdata_));
    return plainPaint().radio(selected, hovered, pressed);
  }

  TogglePaint toggle(bool on, bool hovered, bool pressed) const override {
    if (vtable_.toggle) return fromC(vtable_.toggle(on, hovered, pressed, userdata_));
    return plainPaint().toggle(on, hovered, pressed);
  }

  EntryPaint entry(bool focused, bool hasValue) const override {
    if (vtable_.entry) return fromC(vtable_.entry(focused, hasValue, userdata_));
    return plainPaint().entry(focused, hasValue);
  }

  DropdownPaint dropdown(bool open, bool hasSelection, bool hovered, bool pressed) const override {
    if (vtable_.dropdown) return fromC(vtable_.dropdown(open, hasSelection, hovered, pressed, userdata_));
    return plainPaint().dropdown(open, hasSelection, hovered, pressed);
  }

  SliderPaint slider(bool hovered, bool pressed) const override {
    if (vtable_.slider) return fromC(vtable_.slider(hovered, pressed, userdata_));
    return plainPaint().slider(hovered, pressed);
  }

  ImagePaint image() const override {
    if (vtable_.image) return fromC(vtable_.image(userdata_));
    return plainPaint().image();
  }

  IconPaint icon() const override {
    if (vtable_.icon) return fromC(vtable_.icon(userdata_));
    return plainPaint().icon();
  }

  SidebarPaint sidebar() const override {
    if (vtable_.sidebar) return fromC(vtable_.sidebar(userdata_));
    return plainPaint().sidebar();
  }

private:
  n8v_custom_paint_vtable vtable_{};
  void *userdata_ = nullptr;
};

CustomPaint &mutableCustomPaint() {
  static CustomPaint instance;
  return instance;
}

} // namespace

const Paint &customPaint() { return mutableCustomPaint(); }

void setCustomPaintVTable(const n8v_custom_paint_vtable *vtable, void *userdata) { mutableCustomPaint().setVTable(vtable, userdata); }

} // namespace n8v::detail
