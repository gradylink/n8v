#pragma once

#include <n8v/options.hpp>

#include <clay.h>

#include <cstdint>
#include <string_view>

namespace n8v::detail {

void beginFrame();
void endFrame();

void openFlex(const FlexOptions &options);
void closeFlex();

void entry(const EntryOptions &options);

void dropdown(const DropdownOptions &options);

struct LeafBuilder {
  bool isButton;
  ButtonOptions buttonOptions;
  TextOptions textOptions;

  void operator()(std::string_view label) &&;
};

struct CheckboxBuilder {
  CheckboxOptions options;

  void operator()(std::string_view label) &&;
};

struct RadioBuilder {
  RadioOptions options;

  void operator()(std::string_view label) &&;
};

} // namespace n8v::detail

namespace n8v {

inline detail::LeafBuilder button(ButtonOptions options) { return detail::LeafBuilder{true, std::move(options), {}}; }

inline detail::LeafBuilder text(TextOptions options) { return detail::LeafBuilder{false, {}, std::move(options)}; }

inline detail::CheckboxBuilder checkbox(CheckboxOptions options) { return detail::CheckboxBuilder{std::move(options)}; }

inline detail::RadioBuilder radio(RadioOptions options) { return detail::RadioBuilder{std::move(options)}; }

inline void entry(EntryOptions options) { detail::entry(options); }

inline void dropdown(DropdownOptions options) { detail::dropdown(options); }

} // namespace n8v

#define UI() for (uint8_t n8v_uiLatch = (n8v::detail::beginFrame(), 0); n8v_uiLatch < 1; n8v_uiLatch = 1, n8v::detail::endFrame())

#define flex(...) for (uint8_t n8v_flexLatch = (n8v::detail::openFlex(__VA_ARGS__), 0); n8v_flexLatch < 1; n8v_flexLatch = 1, n8v::detail::closeFlex())
