#include "html_css.hpp"

#include <algorithm>
#include <cstdio>

namespace n8v::detail::html_css {
namespace {

std::string svgColor(const Color &c) {
  char buf[48];
  std::snprintf(buf, sizeof buf, "%%23%02x%02x%02x", (int)c.r, (int)c.g, (int)c.b);
  return buf;
}

std::string svgOpacity(const Color &c) { return cssNumber(c.a / 255.0f); }

std::string transition(float seconds, const char *properties) {
  if (seconds <= 0.0f) return {};
  std::string out = "transition:";
  std::string list = properties;
  size_t start = 0;
  bool first = true;
  while (start <= list.size()) {
    size_t end = list.find(',', start);
    if (end == std::string::npos) end = list.size();
    if (!first) out += ",";
    out += list.substr(start, end - start) + " " + cssNumber(seconds) + "s ease-out";
    first = false;
    start = end + 1;
  }
  return out + ";";
}

std::string fontDecl(FontFamily family, uint16_t size) { return "font-family:" + cssFontFamily(family) + ";font-size:" + cssNumber(size) + "px;line-height:normal;"; }

std::string buttonState(const ButtonPaint &p) {
  return "background:" + cssColor(p.background) + ";color:" + cssColor(p.textColor) + ";border-radius:" + cssRadius(p.cornerRadius) + ";";
}

std::string checkGlyph(const Color &c) {
  if (c.a <= 0.0f) return {};
  return "background-image:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><path d='M3.5 8.5l3 3 6-7' fill='none' stroke='" +
         svgColor(c) + "' stroke-opacity='" + svgOpacity(c) +
         "' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'/></svg>\");background-repeat:no-repeat;background-position:center;background-size:80%;";
}

std::string chevronUrl(const Color &c) {
  return "url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 12 8'><path d='M1.5 1.5l4.5 4.5 4.5-4.5' fill='none' stroke='" + svgColor(c) +
         "' stroke-opacity='" + svgOpacity(c) + "' stroke-width='1.6' stroke-linecap='round' stroke-linejoin='round'/></svg>\")";
}

} // namespace

std::string cssNumber(float v) {
  char buf[48];
  std::snprintf(buf, sizeof buf, "%.3f", v);
  std::string s = buf;
  if (s.find('.') != std::string::npos) {
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
  }
  return s.empty() || s == "-0" ? "0" : s;
}

std::string cssColor(const Color &c) {
  return "rgba(" + std::to_string((int)c.r) + "," + std::to_string((int)c.g) + "," + std::to_string((int)c.b) + "," + cssNumber(c.a / 255.0f) + ")";
}

std::string cssRadius(const CornerRadius &r) {
  return cssNumber(r.topLeft) + "px " + cssNumber(r.topRight) + "px " + cssNumber(r.bottomRight) + "px " + cssNumber(r.bottomLeft) + "px";
}

std::string cssPadding(const Padding &p) {
  return std::to_string(p.top) + "px " + std::to_string(p.right) + "px " + std::to_string(p.bottom) + "px " + std::to_string(p.left) + "px";
}

std::string cssFontFamily(FontFamily family) {
  switch (family) {
  case FontFamily::Roboto:
    return "'n8v-Roboto',sans-serif";
  case FontFamily::Inter:
    return "'n8v-Inter',sans-serif";
  case FontFamily::Selawik:
    return "'n8v-Selawik',sans-serif";
  case FontFamily::DejaVuSans:
    break;
  }
  return "'n8v-DejaVuSans',sans-serif";
}

std::string expand(const std::string &tmpl, const std::string &selector) {
  std::string out;
  out.reserve(tmpl.size() + 16);
  for (char c : tmpl) {
    if (c == '&') out += selector;
    else out += c;
  }
  return out;
}

std::string buttonCss(const Paint &paint, ButtonStyle style) {
  const ButtonPaint normal = paint.button(style, false, false);
  const ButtonPaint hover = paint.button(style, true, false);
  const ButtonPaint pressed = paint.button(style, true, true);
  std::string css = "&{appearance:none;-webkit-appearance:none;border:0;margin:0;cursor:pointer;display:inline-flex;align-items:center;justify-content:center;gap:" +
                    cssNumber(normal.fontSize / 2.0f) + "px;padding:" + cssPadding(normal.padding) + ";" + fontDecl(normal.font, normal.fontSize) + "text-align:center;" +
                    buttonState(normal) + transition(normal.transitionSeconds, "background-color,color,border-radius") + "}\n";
  css += "&:hover{" + buttonState(hover) + "}\n";
  css += "&:active{" + buttonState(pressed) + "}\n";
  css += "&:focus-visible{outline:2px solid " + cssColor(Color{normal.textColor.r, normal.textColor.g, normal.textColor.b, 120}) + ";outline-offset:2px}\n";
  css += "& img{display:block;width:" + cssNumber(normal.fontSize) + "px;height:" + cssNumber(normal.fontSize) + "px;flex:none}\n";
  css += "& img[hidden],& span[hidden]{display:none}\n";
  return css;
}

std::string textCss(const Paint &paint, const TextOptions &options) {
  const TextPaint p = paint.text(options);
  const bool link = !options.url.empty() || options.onClick;
  std::string decoration = link && options.strikethrough ? "underline line-through" : link ? "underline" : options.strikethrough ? "line-through" : "none";
  std::string css = "&{margin:0;color:" + cssColor(p.color) + ";" + fontDecl(p.font, p.fontSize) + "font-weight:" + (options.bold ? "bold" : "normal") +
                    ";font-style:" + (options.italic ? "italic" : "normal") + ";text-decoration:" + decoration + ";overflow-wrap:anywhere;";
  if (link) css += "cursor:pointer;";
  return css + "}\n";
}

std::string checkboxCss(const Paint &paint) {
  const TextPaint label = paint.text({});
  auto state = [&](bool checked, bool hovered, bool pressed) {
    const CheckboxPaint p = paint.checkbox(checked, hovered, pressed);
    return "background-color:" + cssColor(p.background) + ";border-color:" + cssColor(p.borderColor) + ";border-radius:" + cssRadius(p.cornerRadius) + ";" +
           (checked ? checkGlyph(p.checkColor) : std::string());
  };
  const CheckboxPaint base = paint.checkbox(false, false, false);
  const float size = base.indicatorSize > 0.0f ? base.indicatorSize : (float)base.fontSize * 1.2f;
  std::string css = "&{display:inline-flex;align-items:center;gap:" + cssNumber(size * 0.4f) + "px;padding:" + cssPadding(base.padding) +
                    ";cursor:pointer;color:" + cssColor(label.color) + ";" + fontDecl(base.font, base.fontSize) + "}\n";
  css += "& input{appearance:none;-webkit-appearance:none;margin:0;flex:none;cursor:pointer;display:block;width:" + cssNumber(size) + "px;height:" + cssNumber(size) +
         "px;border-style:solid;border-width:" + cssNumber(base.borderWidth) + "px;" + state(false, false, false) +
         transition(base.transitionSeconds, "background-color,border-color,border-radius") + "}\n";
  css += "& input:hover{" + state(false, true, false) + "}\n& input:active{" + state(false, true, true) + "}\n";
  css +=
    "& input:checked{" + state(true, false, false) + "}\n& input:checked:hover{" + state(true, true, false) + "}\n& input:checked:active{" + state(true, true, true) + "}\n";
  css += "& input:focus-visible{outline:2px solid " + cssColor(Color{label.color.r, label.color.g, label.color.b, 120}) + ";outline-offset:2px}\n";
  return css;
}

std::string radioCss(const Paint &paint) {
  const TextPaint label = paint.text({});
  auto state = [&](bool selected, bool hovered, bool pressed) {
    const RadioPaint p = paint.radio(selected, hovered, pressed);
    std::string s = "background-color:" + cssColor(p.background) + ";border-color:" + cssColor(p.borderColor) + ";";
    if (selected && p.dotColor.a > 0.0f) s += "background-image:radial-gradient(circle," + cssColor(p.dotColor) + " 0%," + cssColor(p.dotColor) + " 36%,transparent 40%);";
    return s;
  };
  const RadioPaint base = paint.radio(false, false, false);
  const float size = base.indicatorSize > 0.0f ? base.indicatorSize : (float)base.fontSize * 1.2f;
  std::string css = "&{display:inline-flex;align-items:center;gap:" + cssNumber(size * 0.4f) + "px;padding:" + cssPadding(base.padding) +
                    ";cursor:pointer;color:" + cssColor(label.color) + ";" + fontDecl(base.font, base.fontSize) + "}\n";
  css += "& input{appearance:none;-webkit-appearance:none;margin:0;flex:none;cursor:pointer;display:block;border-radius:50%;width:" + cssNumber(size) +
         "px;height:" + cssNumber(size) + "px;border-style:solid;border-width:" + cssNumber(base.borderWidth) + "px;" + state(false, false, false) +
         transition(base.transitionSeconds, "background-color,border-color") + "}\n";
  css += "& input:hover{" + state(false, true, false) + "}\n& input:active{" + state(false, true, true) + "}\n";
  css +=
    "& input:checked{" + state(true, false, false) + "}\n& input:checked:hover{" + state(true, true, false) + "}\n& input:checked:active{" + state(true, true, true) + "}\n";
  css += "& input:focus-visible{outline:2px solid " + cssColor(Color{label.color.r, label.color.g, label.color.b, 120}) + ";outline-offset:2px}\n";
  return css;
}

std::string toggleCss(const Paint &paint) {
  const TextPaint label = paint.text({});
  const TogglePaint off = paint.toggle(false, false, false);
  const float trackW = off.trackWidth > 0.0f ? off.trackWidth : 40.0f;
  const float trackH = off.trackHeight > 0.0f ? off.trackHeight : 22.0f;
  const float pad = std::max(0.0f, (trackH - off.knobSizeOff) / 2.0f - off.trackBorderWidth);
  auto trackState = [&](bool on, bool h, bool p) {
    const TogglePaint t = paint.toggle(on, h, p);
    return "background-color:" + cssColor(on ? t.trackOnColor : t.trackOffColor) + ";border-color:" + cssColor(t.trackBorderColor) + ";";
  };
  auto knobState = [&](bool on, bool h, bool p) {
    const TogglePaint t = paint.toggle(on, h, p);
    const float size = on ? t.knobSizeOn : t.knobSizeOff;
    std::string s = "width:" + cssNumber(size) + "px;height:" + cssNumber(size) + "px;background-color:" + cssColor(on ? t.knobOnColor : t.knobOffColor) + ";";
    if (on) s += "left:calc(100% - " + cssNumber(size) + "px - " + cssNumber(pad) + "px);";
    if (on && t.showGlyphWhenOn) s += checkGlyph(t.knobGlyphColor);
    return s;
  };
  std::string css = "&{display:inline-flex;align-items:center;gap:" + cssNumber(off.fontSize * 0.5f) + "px;padding:" + cssPadding(off.padding) +
                    ";cursor:pointer;color:" + cssColor(label.color) + ";" + fontDecl(off.font, off.fontSize) + "}\n";
  css += "& input{appearance:none;-webkit-appearance:none;position:relative;margin:0;flex:none;cursor:pointer;display:block;width:" + cssNumber(trackW) +
         "px;height:" + cssNumber(trackH) + "px;border-radius:" + cssNumber(trackH / 2.0f) + "px;border-style:solid;border-width:" + cssNumber(off.trackBorderWidth) + "px;" +
         trackState(false, false, false) + transition(off.transitionSeconds, "background-color,border-color") + "}\n";
  css += "& input::before{content:'';position:absolute;top:50%;left:" + cssNumber(pad) + "px;border-radius:50%;transform:translateY(-50%);" + knobState(false, false, false) +
         transition(off.transitionSeconds, "left,width,height,background-color") + "}\n";
  css += "& input:hover{" + trackState(false, true, false) + "}\n& input:hover::before{" + knobState(false, true, false) + "}\n";
  css += "& input:active::before{" + knobState(false, true, true) + "}\n";
  css += "& input:checked{" + trackState(true, false, false) + "}\n& input:checked::before{" + knobState(true, false, false) + "}\n";
  css += "& input:checked:hover{" + trackState(true, true, false) + "}\n& input:checked:hover::before{" + knobState(true, true, false) + "}\n";
  css += "& input:checked:active::before{" + knobState(true, true, true) + "}\n";
  css += "& input:focus-visible{outline:2px solid " + cssColor(Color{label.color.r, label.color.g, label.color.b, 120}) + ";outline-offset:2px}\n";
  return css;
}

std::string entryCss(const Paint &paint) {
  const EntryPaint idle = paint.entry(false, false);
  const EntryPaint focused = paint.entry(true, false);
  const float borderWidth = idle.borderWidth;
  std::string css = "&{box-sizing:border-box;margin:0;outline:none;min-width:0;background:" + cssColor(idle.background) + ";color:" + cssColor(idle.textColor) +
                    ";border-style:solid;border-width:" + cssNumber(borderWidth) + "px;border-color:" + cssColor(idle.borderColor) +
                    ";border-radius:" + cssRadius(idle.cornerRadius) + ";padding:" + cssPadding(idle.padding) + ";" + fontDecl(idle.font, idle.fontSize) +
                    transition(idle.transitionSeconds, "background-color,border-color,box-shadow") + "}\n";
  css += "&::placeholder{color:" + cssColor(idle.placeholderColor) + ";opacity:1}\n";
  std::string focus = "background:" + cssColor(focused.background) + ";border-color:" + cssColor(focused.borderColor) + ";";
  if (focused.borderWidth > borderWidth) focus += "box-shadow:0 0 0 " + cssNumber(focused.borderWidth - borderWidth) + "px " + cssColor(focused.borderColor) + ";";
  css += "&:focus{" + focus + "}\n";
  return css;
}

std::string dropdownCss(const Paint &paint) {
  const DropdownPaint idle = paint.dropdown(false, false, false, false);
  const DropdownPaint hover = paint.dropdown(false, false, true, false);
  const Color indicator = idle.indicatorColor.a > 0.0f ? idle.indicatorColor : idle.textColor;
  const int rightPad = idle.padding.right + 22;
  std::string css =
    "&{appearance:none;-webkit-appearance:none;box-sizing:border-box;margin:0;min-width:0;cursor:pointer;outline:none;border:0;background-color:" + cssColor(idle.background) +
    ";background-image:" + chevronUrl(indicator) + ";background-repeat:no-repeat;background-position:right " + std::to_string(idle.padding.right + 2) +
    "px center;background-size:12px 8px;color:" + cssColor(idle.textColor) + ";border-radius:" + cssRadius(idle.cornerRadius) +
    ";padding:" + std::to_string(idle.padding.top) + "px " + std::to_string(rightPad) + "px " + std::to_string(idle.padding.bottom) + "px " +
    std::to_string(idle.padding.left) + "px;" + fontDecl(idle.font, idle.fontSize) + transition(idle.transitionSeconds, "background-color") + "}\n";
  css += "&:hover{background-color:" + cssColor(hover.background) + "}\n";
  css += "&:focus-visible{outline:2px solid " + cssColor(Color{idle.textColor.r, idle.textColor.g, idle.textColor.b, 120}) + ";outline-offset:2px}\n";
  css += "& option{background:" + cssColor(idle.popupBackground) + ";color:" + cssColor(idle.textColor) + "}\n";
  return css;
}

std::string sliderCss(const Paint &paint) {
  const SliderPaint p = paint.slider(false, false);
  const SliderPaint hover = paint.slider(true, false);
  const float trackH = p.trackHeight;
  const float thumbMargin = (trackH - p.thumbHeight) / 2.0f;
  const std::string fill = cssColor(p.fillColor), track = cssColor(p.trackColor);
  auto thumb = [&](const SliderPaint &s) {
    return "width:" + cssNumber(s.thumbWidth) + "px;height:" + cssNumber(s.thumbHeight) + "px;border-radius:" + cssNumber(std::min(s.thumbWidth, s.thumbHeight) / 2.0f) +
           "px;background:" + cssColor(s.thumbColor) + ";border:" + cssNumber(s.thumbBorderWidth) + "px solid " + cssColor(s.thumbBorderColor) + ";box-sizing:border-box;";
  };
  std::string css =
    "&{appearance:none;-webkit-appearance:none;margin:0;padding:0;background:transparent;cursor:pointer;min-width:0;height:" + cssNumber(std::max(trackH, p.thumbHeight)) +
    "px;outline:none}\n";
  css += "&::-webkit-slider-runnable-track{height:" + cssNumber(trackH) + "px;border-radius:" + cssNumber(trackH / 2.0f) + "px;background:linear-gradient(to right," + fill +
         " 0," + fill + " var(--n8v-p,50%)," + track + " var(--n8v-p,50%)," + track + " 100%)}\n";
  css += "&::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;margin-top:" + cssNumber(thumbMargin) + "px;" + thumb(p) + "}\n";
  css += "&:hover::-webkit-slider-thumb{" + thumb(hover) + "}\n";
  css += "&::-moz-range-track{height:" + cssNumber(trackH) + "px;border-radius:" + cssNumber(trackH / 2.0f) + "px;background:" + track + "}\n";
  css += "&::-moz-range-progress{height:" + cssNumber(trackH) + "px;border-radius:" + cssNumber(trackH / 2.0f) + "px;background:" + fill + "}\n";
  css += "&::-moz-range-thumb{" + thumb(p) + "}\n";
  return css;
}

std::string panelCss(const Paint &paint, PanelRole role) {
  const PanelPaint normal = paint.panel(role, false, false);
  const PanelPaint hover = paint.panel(role, true, false);
  const PanelPaint pressed = paint.panel(role, true, true);
  auto state = [&](const PanelPaint &p) {
    return "background:" + cssColor(p.background) + ";border-color:" + cssColor(p.borderColor) + ";border-radius:" + cssRadius(p.cornerRadius) + ";";
  };
  std::string css = "&{box-sizing:border-box;border-style:solid;border-width:" + cssNumber(normal.borderWidth) + "px;padding:" + cssPadding(normal.padding) + ";" +
                    state(normal) + "transition:background-color .12s,border-color .12s}\n";
  css += "&:hover{" + state(hover) + "}\n&:active{" + state(pressed) + "}\n";
  return css;
}

std::string sidebarListCss(const Paint &paint) {
  const SidebarPaint p = paint.sidebar();
  return "&{box-sizing:border-box;background:" + cssColor(p.background) + ";border-left:" + cssNumber(p.borderWidth) + "px solid " + cssColor(p.borderColor) +
         ";border-radius:" + cssRadius(p.cornerRadius) + ";padding:" + cssPadding(p.padding) + ";gap:" + cssNumber(p.rowGap) + "px;overflow-y:auto;overflow-x:hidden}\n";
}

std::string imageCss(const CornerRadius &radius) { return "&{display:block;object-fit:fill;border-radius:" + cssRadius(radius) + "}\n"; }

std::string documentCss(const Paint &paint) {
  TextOptions linkOptions;
  linkOptions.url = "https://example.invalid";
  const TextPaint body = paint.text({});
  const TextPaint link = paint.text(linkOptions);
  std::string css = "&{color:" + cssColor(body.color) + ";" + fontDecl(body.font, body.fontSize) + "overflow-wrap:anywhere}\n";
  css += "& h1,& h2,& h3,& h4,& h5,& h6{margin:.6em 0 .3em;line-height:1.2}\n& h1{font-size:2em}\n& h2{font-size:1.5em}\n& h3{font-size:1.25em}\n";
  css += "& p{margin:.5em 0}\n& ul,& ol{margin:.5em 0;padding-left:1.6em}\n";
  css += "& a{color:" + cssColor(link.color) + "}\n";
  css += "& code{font-family:monospace;background:rgba(127,127,127,.18);padding:.1em .3em;border-radius:3px}\n";
  css += "& pre{background:rgba(127,127,127,.18);padding:.6em .8em;border-radius:4px;overflow-x:auto}\n& pre code{background:none;padding:0}\n";
  css += "& blockquote{margin:.5em 0;padding-left:.8em;border-left:3px solid rgba(127,127,127,.5);opacity:.8}\n& hr{border:0;border-top:1px solid rgba(127,127,127,.4)}\n";
  return css;
}

} // namespace n8v::detail::html_css
