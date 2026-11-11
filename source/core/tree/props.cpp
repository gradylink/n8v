#include "core/tree/props.hpp"

#include "core/tree/fields.hpp"

#include <cstring>
#include <string_view>

namespace n8v::detail::tree {
namespace {

struct Encoder {
  std::string &out;

  void bytes(const void *data, size_t size) { out.append(static_cast<const char *>(data), size); }
  template <typename T> void num(const T &value) {
    static_assert(std::is_trivially_copyable_v<T>);
    bytes(&value, sizeof(T));
  }
  template <typename T> void ptr(const T &value) { num(value); }
  void str(const char *const &s) {
    out.push_back(s ? 1 : 0);
    if (!s) return;
    size_t length = std::strlen(s);
    bytes(&length, sizeof length);
    bytes(s, length);
  }
  void strArray(const char *const *const &items, size_t count) {
    for (size_t i = 0; i < count; ++i) str(items[i]);
  }
};

struct Adopter {
  OwnedProps &owned;

  template <typename T> void num(T &) {}
  template <typename T> void ptr(T &) {}
  void str(const char *&s) {
    if (!s) return;
    owned.strings.emplace_back(s);
    s = owned.strings.back().c_str();
  }
  void strArray(const char *const *&items, size_t count) {
    owned.items.clear();
    owned.items.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      const char *item = items[i];
      str(item);
      owned.items.push_back(item);
    }
    items = owned.items.data();
  }
};

} // namespace

OwnedProps adopt(const Node &recorded) {
  OwnedProps owned;
  owned.props = recorded.props;
  owned.label.assign(recorded.label);

  Adopter adopter{owned};
  visitProps(owned.props, adopter);

  Encoder encoder{owned.encoded};
  encoder.num(owned.props.index());
  visitProps(owned.props, encoder);
  encoder.str(owned.label.c_str());
  return owned;
}

const char *idOf(const Node::Props &props) {
  const char *id = nullptr;
  std::visit(
    [&](const auto &options) {
      if constexpr (requires { options.id; }) id = options.id;
    },
    props
  );
  return id && *id ? id : nullptr;
}

BoundValue readBound(const Node::Props &props) {
  return std::visit(
    [](const auto &o) -> BoundValue {
      using T = std::decay_t<decltype(o)>;
      if constexpr (std::is_same_v<T, n8v_checkbox_options> || std::is_same_v<T, n8v_toggle_options> || std::is_same_v<T, n8v_button_options>) {
        return o.checked ? BoundValue(*o.checked) : BoundValue();
      } else if constexpr (std::is_same_v<T, n8v_radio_options> || std::is_same_v<T, n8v_dropdown_options> || std::is_same_v<T, n8v_sidebar_options>) {
        return o.selected ? BoundValue(*o.selected) : BoundValue();
      } else if constexpr (std::is_same_v<T, n8v_slider_options>) {
        return o.value ? BoundValue(*o.value) : BoundValue();
      } else if constexpr (std::is_same_v<T, n8v_entry_options>) {
        return o.value && o.value->data ? BoundValue(std::string(o.value->data, o.value->length)) : BoundValue();
      } else {
        return {};
      }
    },
    props
  );
}

} // namespace n8v::detail::tree
