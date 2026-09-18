#include "html_backend_impl.hpp"

#include <cstdio>
#include <string>

namespace n8v::detail {

std::string cssColor(const Clay_Color &color) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "rgba(%d, %d, %d, %.4f)", (int)color.r, (int)color.g, (int)color.b, color.a / 255.0f);
  return buf;
}

std::string cssColor(const n8v::Color &color) { return cssColor(Clay_Color{color.r, color.g, color.b, color.a}); }

void setCornerRadii(emscripten::val &el, const Clay_CornerRadius &radius) {
  el["style"].set("borderTopLeftRadius", std::to_string(radius.topLeft) + "px");
  el["style"].set("borderTopRightRadius", std::to_string(radius.topRight) + "px");
  el["style"].set("borderBottomRightRadius", std::to_string(radius.bottomRight) + "px");
  el["style"].set("borderBottomLeftRadius", std::to_string(radius.bottomLeft) + "px");
}

namespace {
bool boxEquals(const Clay_BoundingBox &a, const Clay_BoundingBox &b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }

void applyTextStyle(emscripten::val &el, const Clay_RenderCommand &command) {
  const Clay_TextRenderData &text = command.renderData.text;
  auto *flags = static_cast<TextStyleFlags *>(command.userData);
  FontFamily family = flags ? flags->font : FontFamily::DejaVuSans;
  bool bold = flags && flags->bold;
  bool italic = flags && flags->italic;
  bool underline = flags && flags->underline;
  bool strikethrough = flags && flags->strikethrough;

  el["style"].set("color", cssColor(text.textColor));
  el["style"].set("fontFamily", htmlFontFamilyName(family));
  el["style"].set("fontSize", std::to_string(text.fontSize) + "px");
  el["style"].set("fontWeight", std::string(bold ? "bold" : "normal"));
  el["style"].set("fontStyle", std::string(italic ? "italic" : "normal"));
  std::string decoration = underline && strikethrough ? "underline line-through" : underline ? "underline" : strikethrough ? "line-through" : "none";
  el["style"].set("textDecoration", decoration);
  el["style"].set("lineHeight", std::string("normal"));

  std::string_view contents(text.stringContents.chars, (size_t)text.stringContents.length);
  el.set("textContent", std::string(contents));
}
} // namespace

emscripten::val HtmlBackend::getOrCreateElement(uint32_t id, Clay_RenderCommandType type, const char *tag, bool &created) {
  ElementKey key = elementKey(id, type);
  auto it = elementCache_.find(key);
  if (it != elementCache_.end()) {
    touchedThisFrame_[key] = true;
    reorderElement(it->second);
    created = false;
    return it->second;
  }

  emscripten::val el = doc_.call<emscripten::val>("createElement", std::string(tag));
  currentContainer().call<void>("appendChild", el);
  lastAppendedSibling_ = el;
  elementCache_[key] = el;
  touchedThisFrame_[key] = true;
  created = true;
  return el;
}

void HtmlBackend::reorderElement(emscripten::val &el) {
  emscripten::val prevSibling = el["previousSibling"];
  bool inPlace = lastAppendedSibling_.isNull() ? prevSibling.isNull() : prevSibling.strictlyEquals(lastAppendedSibling_);
  if (!inPlace) currentContainer().call<void>("appendChild", el);
  lastAppendedSibling_ = el;
}

void HtmlBackend::positionElement(emscripten::val &el, Clay_BoundingBox &lastBox, const Clay_BoundingBox &box) {
  if (boxEquals(lastBox, box)) return;
  lastBox = box;

  float x = box.x, y = box.y;
  if (!clipStack_.empty()) {
    x -= clipStack_.back().originX;
    y -= clipStack_.back().originY;
  }

  char transform[96];
  std::snprintf(transform, sizeof(transform), "translate(%.2fpx, %.2fpx)", x, y);
  el["style"].set("transform", std::string(transform));
  el["style"].set("width", std::to_string(box.width) + "px");
  el["style"].set("height", std::to_string(box.height) + "px");
}

void HtmlBackend::renderClipStart(const Clay_RenderCommand &command) {
  ElementKey key = elementKey(command.id, CLAY_RENDER_COMMAND_TYPE_SCISSOR_START);
  bool created = false;
  emscripten::val el = getOrCreateElement(command.id, CLAY_RENDER_COMMAND_TYPE_SCISSOR_START, "div", created);
  positionElement(el, elementLastBox_[key], command.boundingBox);

  const Clay_ClipRenderData &clip = command.renderData.clip;
  if (created) {
    el["style"].set("position", std::string("absolute"));
    el["style"].set("overflowX", std::string(clip.horizontal ? "auto" : "visible"));
    el["style"].set("overflowY", std::string(clip.vertical ? "auto" : "visible"));
  }

  clipStack_.push_back(ClipFrame{el, command.boundingBox.x, command.boundingBox.y});
  lastAppendedSibling_ = emscripten::val::null();
}

void HtmlBackend::renderClipEnd() {
  if (!clipStack_.empty()) {
    lastAppendedSibling_ = clipStack_.back().container;
    clipStack_.pop_back();
  }
}

void HtmlBackend::removeUntouchedElements() {
  for (auto it = elementCache_.begin(); it != elementCache_.end();) {
    if (touchedThisFrame_.find(it->first) == touchedThisFrame_.end()) {
      it->second.call<void>("remove");
      elementLastBox_.erase(it->first);
      elementRectSig_.erase(it->first);
      elementTextSig_.erase(it->first);
      elementBorderSig_.erase(it->first);
      elementChevronSig_.erase(it->first);
      elementImageSource_.erase(it->first);
      it = elementCache_.erase(it);
    } else {
      ++it;
    }
  }
}

void HtmlBackend::renderRectangle(const Clay_RenderCommand &command, PendingState &pending) {
  auto *meta = static_cast<NativeWidgetMeta *>(command.userData);

  pending.isCheckbox = meta && meta->kind == NativeWidgetKind::Checkbox;
  pending.isRadio = meta && meta->kind == NativeWidgetKind::Radio;
  pending.isSwitch = meta && meta->kind == NativeWidgetKind::Switch;
  if (pending.isCheckbox || pending.isRadio || pending.isSwitch) {
    pending.indicatorMeta = meta;
    return;
  }

  if (meta && meta->kind == NativeWidgetKind::DropdownChevron) {
    renderDropdownChevron(command);
    return;
  }

  if (meta && meta->kind == NativeWidgetKind::Link && meta->url) {
    pending.linkMeta = meta;
    return;
  }

  if (meta && meta->kind == NativeWidgetKind::Entry) {
    pending.entryMeta = meta;
    pending.entryFieldBox = command.boundingBox;
  }

  ElementKey key = elementKey(command.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
  bool created = false;
  emscripten::val el = getOrCreateElement(command.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE, "div", created);
  positionElement(el, elementLastBox_[key], command.boundingBox);

  RectSignature sig{command.renderData.rectangle.backgroundColor, command.renderData.rectangle.cornerRadius};
  RectSignature &last = elementRectSig_[key];
  if (created || !colorEquals(sig.background, last.background)) el["style"].set("backgroundColor", cssColor(sig.background));
  if (created || !radiusEquals(sig.radius, last.radius)) setCornerRadii(el, sig.radius);
  last = sig;
}

void HtmlBackend::renderText(const Clay_RenderCommand &command, PendingState &pending) {
  if ((pending.isCheckbox || pending.isRadio) && pending.indicatorMeta) {
    renderCheckboxOrRadioIndicator(*pending.indicatorMeta, command.boundingBox, pending.isRadio);
  }
  if (pending.isSwitch && pending.indicatorMeta) {
    renderSwitchIndicator(*pending.indicatorMeta, command.boundingBox);
  }

  if (pending.entryMeta) {
    syncEntryInput(*pending.entryMeta, pending.entryFieldBox, command);
    return;
  }

  if (pending.linkMeta) {
    renderLinkText(*pending.linkMeta, command);
    return;
  }

  ElementKey key = elementKey(command.id, CLAY_RENDER_COMMAND_TYPE_TEXT);
  bool created = false;
  emscripten::val el = getOrCreateElement(command.id, CLAY_RENDER_COMMAND_TYPE_TEXT, "div", created);
  if (created) {
    bool selectable = !pending.isCheckbox && !pending.isRadio;
    el.call<void>("setAttribute", std::string("class"), std::string(selectable ? "n8v-text" : "n8v-text n8v-text-unselectable"));
  }
  positionElement(el, elementLastBox_[key], command.boundingBox);

  const Clay_TextRenderData &text = command.renderData.text;
  auto *flags = static_cast<TextStyleFlags *>(command.userData);
  TextSignature sig{
    text.textColor,
    flags ? flags->font : FontFamily::DejaVuSans,
    text.fontSize,
    flags && flags->bold,
    flags && flags->italic,
    flags && flags->underline,
    flags && flags->strikethrough,
    std::string(text.stringContents.chars, (size_t)text.stringContents.length)
  };
  TextSignature &last = elementTextSig_[key];
  if (
    created || !(colorEquals(sig.color, last.color) && sig.family == last.family && sig.fontSize == last.fontSize && sig.bold == last.bold && sig.italic == last.italic &&
                 sig.underline == last.underline && sig.strikethrough == last.strikethrough && sig.text == last.text)
  ) {
    applyTextStyle(el, command);
    last = std::move(sig);
  }
}

void HtmlBackend::renderLinkText(NativeWidgetMeta &meta, const Clay_RenderCommand &command) {
  ElementKey key = elementKey(command.id, CLAY_RENDER_COMMAND_TYPE_TEXT);
  bool created = false;
  emscripten::val el = getOrCreateElement(command.id, CLAY_RENDER_COMMAND_TYPE_TEXT, "a", created);
  if (created) {
    el.call<void>("setAttribute", std::string("data-n8v-link"), std::string("1"));
    el.call<void>("setAttribute", std::string("class"), std::string("n8v-text"));
    el.call<void>("setAttribute", std::string("href"), *meta.url);
    el["style"].set("cursor", std::string("pointer"));
    el["style"].set("pointerEvents", std::string("auto"));
  }
  positionElement(el, elementLastBox_[key], command.boundingBox);

  const Clay_TextRenderData &text = command.renderData.text;
  auto *flags = static_cast<TextStyleFlags *>(command.userData);
  TextSignature sig{
    text.textColor,
    flags ? flags->font : FontFamily::DejaVuSans,
    text.fontSize,
    flags && flags->bold,
    flags && flags->italic,
    flags && flags->underline,
    flags && flags->strikethrough,
    std::string(text.stringContents.chars, (size_t)text.stringContents.length)
  };
  TextSignature &last = elementTextSig_[key];
  if (
    created || !(colorEquals(sig.color, last.color) && sig.family == last.family && sig.fontSize == last.fontSize && sig.bold == last.bold && sig.italic == last.italic &&
                 sig.underline == last.underline && sig.strikethrough == last.strikethrough && sig.text == last.text)
  ) {
    applyTextStyle(el, command);
    last = std::move(sig);
  }
}

void HtmlBackend::renderBorder(const Clay_RenderCommand &command) {
  const Clay_BorderRenderData &border = command.renderData.border;
  float width = (float)border.width.left;
  if (width <= 0.0f || border.color.a <= 0.0f) return;

  ElementKey key = elementKey(command.id, CLAY_RENDER_COMMAND_TYPE_BORDER);
  bool created = false;
  emscripten::val el = getOrCreateElement(command.id, CLAY_RENDER_COMMAND_TYPE_BORDER, "div", created);
  positionElement(el, elementLastBox_[key], command.boundingBox);
  if (created) el["style"].set("pointerEvents", std::string("none"));

  BorderSignature sig{width, border.color, border.cornerRadius};
  BorderSignature &last = elementBorderSig_[key];
  if (created || width != last.width || !colorEquals(sig.color, last.color)) el["style"].set("border", std::to_string(width) + "px solid " + cssColor(border.color));
  if (created || !radiusEquals(sig.radius, last.radius)) setCornerRadii(el, sig.radius);
  last = sig;
}

void HtmlBackend::present(Clay_RenderCommandArray commands) {
  touchedThisFrame_.clear();
  touchedEntryThisFrame_.clear();
  touchedIndicatorThisFrame_.clear();
  lastAppendedSibling_ = emscripten::val::null();
  clipStack_.clear();

  PendingState pending;

  for (int32_t i = 0; i < commands.length; ++i) {
    Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
    switch (command->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
      renderRectangle(*command, pending);
      break;
    case CLAY_RENDER_COMMAND_TYPE_TEXT:
      renderText(*command, pending);
      pending.clear();
      break;
    case CLAY_RENDER_COMMAND_TYPE_BORDER:
      renderBorder(*command);
      break;
    case CLAY_RENDER_COMMAND_TYPE_IMAGE:
      renderImage(*command);
      pending.clear();
      break;
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      renderClipStart(*command);
      pending.clear();
      break;
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      renderClipEnd();
      pending.clear();
      break;
    default:
      pending.clear();
      break;
    }
  }

  removeUntouchedElements();
  removeUntouchedEntryInputs();
  removeUntouchedIndicators();
}

} // namespace n8v::detail
