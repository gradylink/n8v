#pragma once

#include <n8v/options.hpp>
#include <n8v/style.hpp>
#include <n8v/types.hpp>

#include <string>

namespace n8v::detail::html_css {

std::string cssColor(const Color &c);
std::string cssNumber(float v);
std::string cssRadius(const CornerRadius &r);
std::string cssPadding(const Padding &p);
std::string cssFontFamily(FontFamily family);

std::string expand(const std::string &tmpl, const std::string &selector);

std::string buttonCss(const Paint &paint, ButtonStyle style);
std::string textCss(const Paint &paint, const TextOptions &options);
std::string checkboxCss(const Paint &paint);
std::string radioCss(const Paint &paint);
std::string toggleCss(const Paint &paint);
std::string entryCss(const Paint &paint);
std::string dropdownCss(const Paint &paint);
std::string sliderCss(const Paint &paint);
std::string panelCss(const Paint &paint, PanelRole role);
std::string sidebarListCss(const Paint &paint);
std::string imageCss(const CornerRadius &radius);
std::string documentCss(const Paint &paint);

} // namespace n8v::detail::html_css
