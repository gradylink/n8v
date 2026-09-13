#include "html_backend_impl.hpp"

#include <algorithm>
#include <string>

namespace n8v::detail {

namespace {

constexpr const char *svgNs = "http://www.w3.org/2000/svg";

emscripten::val createSvg(emscripten::val &doc) {
  emscripten::val svg = doc.call<emscripten::val>("createElementNS", std::string(svgNs), std::string("svg"));
  svg.call<void>("setAttribute", std::string("viewBox"), std::string("0 0 100 100"));
  return svg;
}

emscripten::val makeOverlaySvg(emscripten::val &doc, emscripten::val &parent) {
  emscripten::val svg = createSvg(doc);
  svg["style"].set("position", std::string("absolute"));
  svg["style"].set("left", std::string("0"));
  svg["style"].set("top", std::string("0"));
  svg["style"].set("width", std::string("100%"));
  svg["style"].set("height", std::string("100%"));
  parent.call<void>("appendChild", svg);
  return svg;
}

} // namespace

void HtmlBackend::renderCheckboxOrRadioIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox, bool isRadio) {
  float squareSize = meta.indicatorSize > 0.0f ? meta.indicatorSize : labelBox.height;
  float gap = squareSize * 0.4f;
  Clay_BoundingBox squareBox{labelBox.x - squareSize - gap, labelBox.y, squareSize, squareSize};

  touchedIndicatorThisFrame_[meta.ordinal] = true;

  if (isRadio) {
    renderRadioIndicator(meta, squareBox);
  } else {
    renderCheckboxIndicator(meta, squareBox);
  }
}

void HtmlBackend::renderRadioIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &squareBox) {
  auto it = indicatorElements_.find(meta.ordinal);
  emscripten::val el;
  bool created = false;
  if (it != indicatorElements_.end()) {
    el = it->second;
    reorderElement(el);
  } else {
    el = createSvg(doc_);
    root_.call<void>("appendChild", el);
    lastAppendedSibling_ = el;
    indicatorElements_[meta.ordinal] = el;
    created = true;
  }

  positionElement(el, indicatorLastBox_[meta.ordinal], squareBox);

  float scale = std::clamp(meta.indicatorGlyphScale, 0.0f, 1.0f);
  IndicatorSignature sig{meta.indicatorFillColor, meta.indicatorBorderColor, meta.indicatorGlyphColor, meta.indicatorBorderWidth, 0.0f, scale};
  IndicatorSignature &last = indicatorSig_[meta.ordinal];
  if (
    !created && colorEquals(sig.fill, last.fill) && colorEquals(sig.border, last.border) && colorEquals(sig.glyph, last.glyph) && sig.borderWidth == last.borderWidth &&
    sig.glyphScale == last.glyphScale
  ) {
    return;
  }
  last = sig;

  float strokeWidth = squareBox.width > 0.0f ? (meta.indicatorBorderWidth / squareBox.width) * 100.0f : 0.0f;
  float ringRadius = 50.0f - strokeWidth * 0.5f;

  std::string svgContent = "<circle cx='50' cy='50' r='" + std::to_string(ringRadius) + "' fill='" + cssColor(meta.indicatorFillColor) + "'";
  if (strokeWidth > 0.0f) svgContent += " stroke='" + cssColor(meta.indicatorBorderColor) + "' stroke-width='" + std::to_string(strokeWidth) + "'";
  svgContent += "/>";

  if (scale > 0.01f) {
    float dotRadius = 28.125f * scale;
    svgContent += "<circle cx='50' cy='50' r='" + std::to_string(dotRadius) + "' fill='" + cssColor(meta.indicatorGlyphColor) + "'/>";
  }

  el.set("innerHTML", svgContent);
}

void HtmlBackend::renderCheckboxIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &squareBox) {
  auto it = indicatorElements_.find(meta.ordinal);
  emscripten::val el;
  bool created = false;
  if (it != indicatorElements_.end()) {
    el = it->second;
    reorderElement(el);
  } else {
    el = doc_.call<emscripten::val>("createElement", std::string("div"));
    root_.call<void>("appendChild", el);
    lastAppendedSibling_ = el;
    makeOverlaySvg(doc_, el);
    indicatorElements_[meta.ordinal] = el;
    created = true;
  }

  positionElement(el, indicatorLastBox_[meta.ordinal], squareBox);

  IndicatorSignature sig{meta.indicatorFillColor, meta.indicatorBorderColor, meta.indicatorGlyphColor, meta.indicatorBorderWidth, meta.indicatorCornerRadius, 0.0f};
  IndicatorSignature &last = indicatorSig_[meta.ordinal];
  if (
    !created && colorEquals(sig.fill, last.fill) && colorEquals(sig.border, last.border) && colorEquals(sig.glyph, last.glyph) && sig.borderWidth == last.borderWidth &&
    sig.cornerRadius == last.cornerRadius
  ) {
    return;
  }
  last = sig;

  el["style"].set("backgroundColor", cssColor(meta.indicatorFillColor));
  Clay_CornerRadius cornerRadius{meta.indicatorCornerRadius, meta.indicatorCornerRadius, meta.indicatorCornerRadius, meta.indicatorCornerRadius};
  setCornerRadii(el, cornerRadius);
  el["style"].set(
    "border",
    meta.indicatorBorderWidth > 0.0f ? (std::to_string(meta.indicatorBorderWidth) + "px solid " + cssColor(meta.indicatorBorderColor)) : std::string("none")
  );

  emscripten::val svg = el["firstChild"];
  std::string glyph;
  if (meta.indicatorGlyphColor.a > 0.5f) {
    // Same checkmark path as Sdl2Backend::drawCheckmark, expressed as fractions of a 100x100 viewBox.
    glyph = "<polyline points='15,45 40,70 85,25' fill='none' stroke='" + cssColor(meta.indicatorGlyphColor) +
            "' stroke-width='12' stroke-linecap='round' stroke-linejoin='round'/>";
  }
  svg.set("innerHTML", glyph);
}

void HtmlBackend::renderSwitchIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox) {
  float trackW = meta.switchTrackWidth, trackH = meta.switchTrackHeight;
  float gap = trackH * 0.5f;
  Clay_BoundingBox trackBox{labelBox.x - trackW - gap, labelBox.y + (labelBox.height - trackH) * 0.5f, trackW, trackH};

  touchedIndicatorThisFrame_[meta.ordinal] = true;

  auto it = switchElements_.find(meta.ordinal);
  emscripten::val el;
  bool created = false;
  if (it != switchElements_.end()) {
    el = it->second;
    reorderElement(el);
  } else {
    el = createSvg(doc_);
    el.call<void>("setAttribute", std::string("preserveAspectRatio"), std::string("none"));
    root_.call<void>("appendChild", el);
    lastAppendedSibling_ = el;
    switchElements_[meta.ordinal] = el;
    created = true;
  }

  positionElement(el, switchLastBox_[meta.ordinal], trackBox);

  SwitchSignature sig{
    meta.switchTrackColor,
    meta.switchTrackBorderColor,
    meta.switchKnobColor,
    meta.switchKnobGlyphColor,
    meta.switchTrackBorderWidth,
    meta.switchKnobPosition,
    meta.switchKnobSize,
    meta.switchGlyphScale,
    trackW,
    trackH,
  };
  SwitchSignature &last = switchSig_[meta.ordinal];
  if (
    !created && colorEquals(sig.trackColor, last.trackColor) && colorEquals(sig.trackBorderColor, last.trackBorderColor) && colorEquals(sig.knobColor, last.knobColor) &&
    colorEquals(sig.glyphColor, last.glyphColor) && sig.trackBorderWidth == last.trackBorderWidth && sig.knobPosition == last.knobPosition && sig.knobSize == last.knobSize &&
    sig.glyphScale == last.glyphScale && sig.trackWidth == last.trackWidth && sig.trackHeight == last.trackHeight
  ) {
    return;
  }
  last = sig;

  el.call<void>("setAttribute", std::string("viewBox"), std::string("0 0 ") + std::to_string(trackW) + " " + std::to_string(trackH));

  float strokeWidth = meta.switchTrackBorderWidth;
  float pillR = trackH / 2.0f;
  std::string svgContent = "<rect x='" + std::to_string(strokeWidth / 2.0f) + "' y='" + std::to_string(strokeWidth / 2.0f) + "' width='" +
                           std::to_string(trackW - strokeWidth) + "' height='" + std::to_string(trackH - strokeWidth) + "' rx='" + std::to_string(pillR) + "' fill='" +
                           cssColor(meta.switchTrackColor) + "'";
  if (strokeWidth > 0.0f) svgContent += " stroke='" + cssColor(meta.switchTrackBorderColor) + "' stroke-width='" + std::to_string(strokeWidth) + "'";
  svgContent += "/>";

  float knobInset = std::max((trackH - meta.switchKnobSize) * 0.5f, 0.0f);
  float leftX = meta.switchKnobSize * 0.5f + knobInset;
  float rightX = trackW - meta.switchKnobSize * 0.5f - knobInset;
  float knobCx = leftX + (rightX - leftX) * std::clamp(meta.switchKnobPosition, 0.0f, 1.0f);
  float knobCy = trackH / 2.0f;
  svgContent += "<circle cx='" + std::to_string(knobCx) + "' cy='" + std::to_string(knobCy) + "' r='" + std::to_string(meta.switchKnobSize / 2.0f) + "' fill='" +
                cssColor(meta.switchKnobColor) + "'/>";

  if (meta.switchGlyphScale > 0.01f) {
    float r = meta.switchKnobSize / 2.0f;
    float x0 = knobCx - r * 0.55f, y0 = knobCy + 0.0f * r;
    float xm = knobCx - r * 0.1f, ym = knobCy + r * 0.4f;
    float x1 = knobCx + r * 0.55f, y1 = knobCy - r * 0.35f;
    svgContent += "<polyline points='" + std::to_string(x0) + "," + std::to_string(y0) + " " + std::to_string(xm) + "," + std::to_string(ym) + " " + std::to_string(x1) + "," +
                  std::to_string(y1) + "' fill='none' stroke='" + cssColor(meta.switchKnobGlyphColor) + "' stroke-opacity='" + std::to_string(meta.switchGlyphScale) +
                  "' stroke-width='" + std::to_string(r * 0.22f) + "' stroke-linecap='round' stroke-linejoin='round'/>";
  }

  el.set("innerHTML", svgContent);
}

void HtmlBackend::removeUntouchedIndicators() {
  for (auto it = indicatorElements_.begin(); it != indicatorElements_.end();) {
    if (touchedIndicatorThisFrame_.find(it->first) == touchedIndicatorThisFrame_.end()) {
      it->second.call<void>("remove");
      indicatorLastBox_.erase(it->first);
      indicatorSig_.erase(it->first);
      it = indicatorElements_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = switchElements_.begin(); it != switchElements_.end();) {
    if (touchedIndicatorThisFrame_.find(it->first) == touchedIndicatorThisFrame_.end()) {
      it->second.call<void>("remove");
      switchLastBox_.erase(it->first);
      switchSig_.erase(it->first);
      it = switchElements_.erase(it);
    } else {
      ++it;
    }
  }
}

void HtmlBackend::renderDropdownChevron(const Clay_RenderCommand &command) {
  auto *meta = static_cast<NativeWidgetMeta *>(command.userData);

  ElementKey key = elementKey(command.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
  bool created = false;
  emscripten::val el = getOrCreateElement(command.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE, "div", created);
  positionElement(el, elementLastBox_[key], command.boundingBox);
  if (created) {
    el["style"].set("backgroundColor", std::string("transparent"));
    makeOverlaySvg(doc_, el);
  }

  bool pointsUp = meta && meta->chevronPointsUp;
  n8v::Color color = meta ? meta->chevronColor : n8v::Color{0, 0, 0, 255};
  ChevronSignature sig{pointsUp, color, true};
  ChevronSignature &last = elementChevronSig_[key];
  if (!created && last.set && last.pointsUp == sig.pointsUp && colorEquals(last.color, sig.color)) return;
  last = sig;

  emscripten::val svg = el["firstChild"];
  const float halfW = 30.0f, top = 35.0f, bottom = 65.0f;
  float apexY = pointsUp ? top : bottom;
  float baseY = pointsUp ? bottom : top;
  std::string points =
    "50," + std::to_string(apexY) + " " + std::to_string(50.0f - halfW) + "," + std::to_string(baseY) + " " + std::to_string(50.0f + halfW) + "," + std::to_string(baseY);
  svg.set("innerHTML", "<polygon points='" + points + "' fill='" + cssColor(color) + "'/>");
}

} // namespace n8v::detail
