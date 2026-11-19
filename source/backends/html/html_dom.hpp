#pragma once

#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/val.h>
#else
#include <memory>
#include <vector>
#endif

namespace n8v::detail::html {

#ifdef __EMSCRIPTEN__

class Element {
public:
  Element() = default;
  explicit Element(emscripten::val v) : v_(std::move(v)) {}

  static Element create(const char *tag) { return Element(emscripten::val::global("document").call<emscripten::val>("createElement", std::string(tag))); }

  bool isNull() const { return v_.isUndefined() || v_.isNull(); }
  emscripten::val &raw() { return v_; }
  const emscripten::val &raw() const { return v_; }

  void setAttr(const char *name, const std::string &value) { v_.call<void>("setAttribute", std::string(name), value); }
  void removeAttr(const char *name) { v_.call<void>("removeAttribute", std::string(name)); }
  void setClassName(const std::string &name) { v_.set("className", name); }
  void setStyleText(const std::string &cssText) { v_["style"].set("cssText", cssText); }
  /** @param property kebab-case CSS property; an empty value removes it. */
  void setStyle(const char *property, const std::string &value) { v_["style"].call<void>("setProperty", std::string(property), value); }

  void setText(const std::string &text) { v_.set("textContent", text); }
  void setHtml(const std::string &html) { v_.set("innerHTML", html); }
  void clear() { v_.set("innerHTML", std::string()); }

  void setValue(const std::string &value) { v_.set("value", value); }
  std::string value() const { return v_["value"].as<std::string>(); }
  void setSelectedIndex(int index) { v_.set("selectedIndex", index); }

  void setBool(const char *property, bool on) { v_.set(property, on); }
  void setString(const char *property, const std::string &value) { v_.set(property, value); }
  void setNumber(const char *property, int value) { v_.set(property, value); }

  void append(const Element &child) { v_.call<void>("appendChild", child.v_); }
  void insertBefore(const Element &child, const Element &reference) { v_.call<void>("insertBefore", child.v_, reference.isNull() ? emscripten::val::null() : reference.v_); }
  void remove() { v_.call<void>("remove"); }
  bool hasParent() const { return !v_["parentNode"].isNull(); }
  std::string tag() const { return v_["localName"].as<std::string>(); }
  void replaceWith(const Element &other) { v_.call<void>("replaceWith", other.v_); }

  double imageKey() const {
    const emscripten::val key = v_["n8vImage"];
    return key.isUndefined() ? -1.0 : key.as<double>();
  }
  void setImageKey(double key) { v_.set("n8vImage", key); }

private:
  emscripten::val v_ = emscripten::val::undefined();
};

#else

struct DomNode;

class Element {
public:
  Element() = default;
  explicit Element(std::shared_ptr<DomNode> node) : n_(std::move(node)) {}

  static Element create(const char *tag);

  bool isNull() const { return !n_; }
  DomNode *node() const { return n_.get(); }

  void setAttr(const char *name, const std::string &value);
  void removeAttr(const char *name);
  void setClassName(const std::string &name);
  void setStyleText(const std::string &cssText);
  void setStyle(const char *property, const std::string &value);

  void setText(const std::string &text);
  void setHtml(const std::string &html);
  void clear();

  void setValue(const std::string &value);
  std::string value() const;
  void setSelectedIndex(int index);

  void setBool(const char *property, bool on);
  void setString(const char *property, const std::string &value);
  void setNumber(const char *property, int value);

  void append(const Element &child);
  void insertBefore(const Element &child, const Element &reference);
  void remove();
  bool hasParent() const;
  std::string tag() const;
  void replaceWith(const Element &other);

  double imageKey() const;
  void setImageKey(double key);

  std::string html() const;

private:
  std::shared_ptr<DomNode> n_;
};

#endif

} // namespace n8v::detail::html
