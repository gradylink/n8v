#include <n8v/n8v_c.h>

#include <n8v/backend.hpp>
#include <n8v/style.hpp>
#include <n8v/ui.hpp>

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

std::string_view toView(const char *s) { return s ? std::string_view(s) : std::string_view{}; }

n8v::Direction toDirection(n8v_direction d) {
  switch (d) {
  case N8V_DIRECTION_HORIZONTAL:
    return n8v::Direction::Horizontal;
  case N8V_DIRECTION_VERTICAL:
    return n8v::Direction::Vertical;
  }
  return n8v::Direction::Horizontal;
}

n8v::Align toAlign(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_START:
    return n8v::Align::Start;
  case N8V_ALIGN_CENTER:
    return n8v::Align::Center;
  case N8V_ALIGN_END:
    return n8v::Align::End;
  }
  return n8v::Align::Start;
}

n8v::ButtonStyle toButtonStyle(n8v_button_style s) {
  switch (s) {
  case N8V_BUTTON_STYLE_PRIMARY:
    return n8v::ButtonStyle::Primary;
  case N8V_BUTTON_STYLE_SECONDARY:
    return n8v::ButtonStyle::Secondary;
  }
  return n8v::ButtonStyle::Primary;
}

n8v::SizingMode toSizingMode(n8v_sizing_mode m) {
  switch (m) {
  case N8V_SIZING_FIT:
    return n8v::SizingMode::Fit;
  case N8V_SIZING_GROW:
    return n8v::SizingMode::Grow;
  case N8V_SIZING_FIXED:
    return n8v::SizingMode::Fixed;
  case N8V_SIZING_PERCENT:
    return n8v::SizingMode::Percent;
  }
  return n8v::SizingMode::Fit;
}

n8v::Sizing toSizing(n8v_sizing s) { return n8v::Sizing{toSizingMode(s.mode), s.value, s.min, s.max}; }

n8v::Color toColor(n8v_color c) { return n8v::Color{c.r, c.g, c.b, c.a}; }

n8v::Padding toPadding(n8v_padding p) { return n8v::Padding{p.left, p.right, p.top, p.bottom}; }

n8v::StyleFamily toStyleFamily(n8v_style_family f) {
  switch (f) {
  case N8V_STYLE_FAMILY_PLAIN:
    return n8v::StyleFamily::Plain;
  case N8V_STYLE_FAMILY_MATERIAL:
    return n8v::StyleFamily::Material;
  case N8V_STYLE_FAMILY_CUPERTINO:
    return n8v::StyleFamily::Cupertino;
  case N8V_STYLE_FAMILY_FLUENT:
    return n8v::StyleFamily::Fluent;
  }
  return n8v::StyleFamily::Plain;
}

n8v_style_family fromStyleFamily(n8v::StyleFamily f) {
  switch (f) {
  case n8v::StyleFamily::Plain:
    return N8V_STYLE_FAMILY_PLAIN;
  case n8v::StyleFamily::Material:
    return N8V_STYLE_FAMILY_MATERIAL;
  case n8v::StyleFamily::Cupertino:
    return N8V_STYLE_FAMILY_CUPERTINO;
  case n8v::StyleFamily::Fluent:
    return N8V_STYLE_FAMILY_FLUENT;
  }
  return N8V_STYLE_FAMILY_PLAIN;
}

void ensureCapacity(n8v_string_buf &buf, size_t needed) {
  if (needed <= buf.capacity) return;
  size_t newCap = buf.capacity == 0 ? 16 : buf.capacity;
  while (newCap < needed) newCap *= 2;
  char *newData = static_cast<char *>(std::realloc(buf.data, newCap));
  if (!newData) return;
  buf.data = newData;
  buf.capacity = newCap;
}

void writeShadowToBuf(const std::string &shadow, n8v_string_buf &buf) {
  ensureCapacity(buf, shadow.size() + 1);
  if (buf.capacity < shadow.size() + 1) return;
  std::memcpy(buf.data, shadow.data(), shadow.size());
  buf.data[shadow.size()] = '\0';
  buf.length = shadow.size();
}

std::unordered_map<n8v_string_buf *, std::string> entryShadows;

n8v::ButtonOptions pendingButtonOpts;
n8v::TextOptions pendingTextOpts;
n8v::CheckboxOptions pendingCheckboxOpts;
n8v::RadioOptions pendingRadioOpts;

} // namespace

extern "C" {

void n8v_string_buf_init(n8v_string_buf *buf) {
  if (!buf) return;
  buf->data = static_cast<char *>(std::malloc(1));
  if (buf->data) buf->data[0] = '\0';
  buf->length = 0;
  buf->capacity = buf->data ? 1 : 0;
}

void n8v_string_buf_free(n8v_string_buf *buf) {
  if (!buf) return;
  std::free(buf->data);
  buf->data = nullptr;
  buf->length = 0;
  buf->capacity = 0;
  entryShadows.erase(buf);
}

const char *n8v_string_buf_cstr(const n8v_string_buf *buf) { return (buf && buf->data) ? buf->data : ""; }

bool n8v_initialize(int width, int height, const char *title) { return n8v::activeBackend().initialize(width, height, toView(title)); }

bool n8v_pump_events(void) { return n8v::activeBackend().pumpEvents(); }

void n8v_shutdown(void) { n8v::activeBackend().shutdown(); }

void n8v_set_style_family(n8v_style_family family) { n8v::setStyleFamily(toStyleFamily(family)); }

n8v_style_family n8v_active_style_family(void) { return fromStyleFamily(n8v::activeStyleFamily()); }

void n8v_begin_frame(void) { n8v::detail::beginFrame(); }

void n8v_end_frame(void) { n8v::detail::endFrame(); }

void n8v_open_flex(n8v_flex_options options) {
  n8v::FlexOptions o;
  o.direction = toDirection(options.direction);
  o.gap = options.gap;
  o.padding = toPadding(options.padding);
  o.hAlign = toAlign(options.h_align);
  o.vAlign = toAlign(options.v_align);
  o.width = toSizing(options.width);
  o.height = toSizing(options.height);
  n8v::detail::openFlex(o);
}

void n8v_close_flex(void) { n8v::detail::closeFlex(); }

void _n8v_set_button_opts(n8v_button_options opts) {
  pendingButtonOpts = n8v::ButtonOptions{};
  pendingButtonOpts.style = toButtonStyle(opts.style);
  if (opts.on_click) {
    n8v_click_fn fn = opts.on_click;
    void *userdata = opts.on_click_userdata;
    pendingButtonOpts.onClick = [fn, userdata] { fn(userdata); };
  }
}

void _n8v_button_commit(const char *label) { n8v::detail::LeafBuilder{true, std::move(pendingButtonOpts), {}}(toView(label)); }

void _n8v_set_text_opts(n8v_text_options opts) {
  pendingTextOpts = n8v::TextOptions{};
  pendingTextOpts.bold = opts.bold;
  pendingTextOpts.italic = opts.italic;
  pendingTextOpts.url = toView(opts.url);
  pendingTextOpts.color = toColor(opts.color);
}

void _n8v_text_commit(const char *label) { n8v::detail::LeafBuilder{false, {}, std::move(pendingTextOpts)}(toView(label)); }

void _n8v_set_checkbox_opts(n8v_checkbox_options opts) {
  pendingCheckboxOpts = n8v::CheckboxOptions{};
  pendingCheckboxOpts.checked = opts.checked;
  if (opts.on_change) {
    n8v_bool_change_fn fn = opts.on_change;
    void *userdata = opts.on_change_userdata;
    pendingCheckboxOpts.onChange = [fn, userdata](bool value) { fn(value, userdata); };
  }
}

void _n8v_checkbox_commit(const char *label) { n8v::detail::CheckboxBuilder{std::move(pendingCheckboxOpts)}(toView(label)); }

void _n8v_set_radio_opts(n8v_radio_options opts) {
  pendingRadioOpts = n8v::RadioOptions{};
  pendingRadioOpts.selected = opts.selected;
  pendingRadioOpts.value = opts.value;
  if (opts.on_change) {
    n8v_int_change_fn fn = opts.on_change;
    void *userdata = opts.on_change_userdata;
    pendingRadioOpts.onChange = [fn, userdata](int value) { fn(value, userdata); };
  }
}

void _n8v_radio_commit(const char *label) { n8v::detail::RadioBuilder{std::move(pendingRadioOpts)}(toView(label)); }

void n8v_entry(n8v_entry_options options) {
  if (!options.value) return;
  n8v_string_buf &buf = *options.value;
  if (!buf.data) n8v_string_buf_init(&buf);

  std::string &shadow = entryShadows[&buf];
  std::string_view currentBufView(buf.data ? buf.data : "", buf.length);
  if (currentBufView != shadow) shadow.assign(currentBufView);

  n8v::EntryOptions cppOpts;
  cppOpts.value = &shadow;
  cppOpts.placeholder = toView(options.placeholder);
  cppOpts.password = options.password;

  n8v_text_change_fn fn = options.on_change;
  void *userdata = options.on_change_userdata;
  n8v_string_buf *bufPtr = &buf;
  cppOpts.onChange = [fn, userdata, bufPtr](std::string_view newValue) {
    writeShadowToBuf(std::string(newValue), *bufPtr);
    if (fn) fn(bufPtr->data, bufPtr->length, userdata);
  };

  n8v::detail::entry(cppOpts);
}

void n8v_dropdown(n8v_dropdown_options options) {
  n8v::DropdownOptions cppOpts;
  cppOpts.items.reserve(options.item_count);
  for (size_t i = 0; i < options.item_count; ++i) {
    cppOpts.items.emplace_back(toView(options.items ? options.items[i] : nullptr));
  }
  cppOpts.selected = options.selected;
  cppOpts.placeholder = toView(options.placeholder);
  if (options.on_change) {
    n8v_int_change_fn fn = options.on_change;
    void *userdata = options.on_change_userdata;
    cppOpts.onChange = [fn, userdata](int value) { fn(value, userdata); };
  }
  n8v::detail::dropdown(cppOpts);
}

void n8v_slider(n8v_slider_options options) {
  n8v::SliderOptions cppOpts;
  cppOpts.value = options.value;
  cppOpts.min = options.min;
  cppOpts.max = options.max;
  if (options.on_change) {
    n8v_float_change_fn fn = options.on_change;
    void *userdata = options.on_change_userdata;
    cppOpts.onChange = [fn, userdata](float value) { fn(value, userdata); };
  }
  n8v::detail::slider(cppOpts);
}

} // extern "C"
