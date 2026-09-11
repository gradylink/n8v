#include "html_backend_impl.hpp"

#include "core/ui_core_internal.hpp"

#include <string>

namespace n8v::detail {

void HtmlBackend::syncEntryInput(NativeWidgetMeta &meta, const Clay_BoundingBox &fieldBox, const Clay_RenderCommand &textCommand) {
  touchedEntryThisFrame_[meta.ordinal] = true;

  EntryBinding &binding = entryBindings_[meta.ordinal];
  binding.entryValue = meta.entryValue;
  binding.entryBuf = meta.entryBuf;
  binding.onChange = meta.onEntryChange;
  binding.onChangeUserdata = meta.onEntryChangeUserdata;

  auto it = entryElements_.find(meta.ordinal);
  emscripten::val el;
  bool created = false;
  if (it != entryElements_.end()) {
    el = it->second;
    reorderElement(el);
  } else {
    el = doc_.call<emscripten::val>("createElement", std::string("input"));
    el.call<void>("setAttribute", std::string("data-n8v-ordinal"), std::to_string(meta.ordinal));
    root_.call<void>("appendChild", el);
    lastAppendedSibling_ = el;
    entryElements_[meta.ordinal] = el;
    created = true;
  }

  positionElement(el, entryLastBox_[meta.ordinal], fieldBox);

  const Clay_TextRenderData &text = textCommand.renderData.text;
  auto *flags = static_cast<TextStyleFlags *>(textCommand.userData);
  FontFamily family = flags ? flags->font : FontFamily::DejaVuSans;

  std::string_view rendered(text.stringContents.chars, (size_t)text.stringContents.length);
  bool suppressPlaceholder = rendered == " ";

  EntryStyleSignature sig;
  sig.password = meta.password;
  sig.family = family;
  sig.fontSize = text.fontSize;
  sig.color = text.textColor;
  sig.padLeft = textCommand.boundingBox.x - fieldBox.x;
  sig.padTop = textCommand.boundingBox.y - fieldBox.y;
  sig.hasPlaceholderAttr = !suppressPlaceholder && meta.placeholder != nullptr;
  sig.placeholderAttr = sig.hasPlaceholderAttr ? *meta.placeholder : std::string{};

  EntryStyleSignature &lastSig = entryStyleSig_[meta.ordinal];
  bool styleChanged = created || sig.password != lastSig.password || sig.family != lastSig.family || sig.fontSize != lastSig.fontSize ||
                      !colorEquals(sig.color, lastSig.color) || sig.padLeft != lastSig.padLeft || sig.padTop != lastSig.padTop ||
                      sig.hasPlaceholderAttr != lastSig.hasPlaceholderAttr || sig.placeholderAttr != lastSig.placeholderAttr;
  if (styleChanged) {
    el.set("type", std::string(meta.password ? "password" : "text"));
    el["style"].set("fontFamily", htmlFontFamilyName(family));
    el["style"].set("fontSize", std::to_string(text.fontSize) + "px");
    el["style"].set("color", cssColor(text.textColor));
    el["style"].set("caretColor", cssColor(text.textColor));

    el["style"].set("paddingLeft", std::to_string(sig.padLeft) + "px");
    el["style"].set("paddingRight", std::to_string(sig.padLeft) + "px");
    el["style"].set("paddingTop", std::to_string(sig.padTop) + "px");
    el["style"].set("paddingBottom", std::to_string(sig.padTop) + "px");

    if (sig.hasPlaceholderAttr) el.call<void>("setAttribute", std::string("placeholder"), sig.placeholderAttr);
    else el.call<void>("removeAttribute", std::string("placeholder"));

    lastSig = std::move(sig);
  }

  std::string currentValue = meta.entryValue ? *meta.entryValue : std::string{};
  emscripten::val active = doc_["activeElement"];
  bool focused = active.strictlyEquals(el);
  if (!focused && el["value"].as<std::string>() != currentValue) el.set("value", currentValue);
}

void HtmlBackend::onEntryInput(int ordinal, std::string_view value) {
  auto it = entryBindings_.find(ordinal);
  if (it == entryBindings_.end() || !it->second.entryValue) return;

  EntryBinding &binding = it->second;
  binding.entryValue->assign(value);
  if (binding.entryBuf) ui_internal::writeToStringBuf(*binding.entryValue, *binding.entryBuf);
  if (binding.onChange) binding.onChange(binding.entryValue->c_str(), binding.entryValue->size(), binding.onChangeUserdata);
}

void HtmlBackend::removeUntouchedEntryInputs() {
  for (auto it = entryElements_.begin(); it != entryElements_.end();) {
    if (touchedEntryThisFrame_.find(it->first) == touchedEntryThisFrame_.end()) {
      it->second.call<void>("remove");
      entryBindings_.erase(it->first);
      entryLastBox_.erase(it->first);
      entryStyleSig_.erase(it->first);
      it = entryElements_.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace n8v::detail
