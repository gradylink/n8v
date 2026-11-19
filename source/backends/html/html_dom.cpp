#ifndef __EMSCRIPTEN__

#include "html_dom.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace n8v::detail::html {

struct DomNode {
  std::string tag;
  std::string className;
  std::string styleBase;
  std::vector<std::pair<std::string, std::string>> styleOverrides;
  std::vector<std::pair<std::string, std::string>> attrs;
  std::string text;
  std::string rawHtml;
  std::vector<std::shared_ptr<DomNode>> children;
  DomNode *parent = nullptr;
  double imageKey = -1.0;
};

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}

void setAttribute(DomNode &n, const std::string &name, const std::string &value) {
  for (auto &[key, current] : n.attrs) {
    if (key == name) {
      current = value;
      return;
    }
  }
  n.attrs.emplace_back(name, value);
}

void removeAttribute(DomNode &n, const std::string &name) {
  n.attrs.erase(std::remove_if(n.attrs.begin(), n.attrs.end(), [&](const auto &kv) { return kv.first == name; }), n.attrs.end());
}

const std::string *findAttribute(const DomNode &n, const std::string &name) {
  for (const auto &[key, value] : n.attrs)
    if (key == name) return &value;
  return nullptr;
}

void detach(DomNode &child) {
  if (!child.parent) return;
  auto &siblings = child.parent->children;
  siblings.erase(std::remove_if(siblings.begin(), siblings.end(), [&](const auto &c) { return c.get() == &child; }), siblings.end());
  child.parent = nullptr;
}

std::string escape(const std::string &in, bool attribute) {
  std::string out;
  out.reserve(in.size());
  for (char c : in) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += attribute ? "&quot;" : "\"";
      break;
    default:
      out += c;
    }
  }
  return out;
}

bool isVoid(const std::string &tag) { return tag == "input" || tag == "img" || tag == "hr" || tag == "br" || tag == "source" || tag == "meta" || tag == "link"; }

std::string styleText(const DomNode &n) {
  std::string out = n.styleBase;
  for (const auto &[key, value] : n.styleOverrides) out += key + ":" + value + ";";
  return out;
}

void serialize(const DomNode &n, std::string &out) {
  out += "<" + n.tag;
  if (!n.className.empty()) out += " class=\"" + escape(n.className, true) + "\"";
  if (const std::string style = styleText(n); !style.empty()) out += " style=\"" + escape(style, true) + "\"";
  for (const auto &[key, value] : n.attrs) {
    out += " " + key;
    if (!value.empty()) out += "=\"" + escape(value, true) + "\"";
  }
  out += ">";
  if (isVoid(n.tag)) return;
  out += escape(n.text, false);
  out += n.rawHtml;
  for (const auto &child : n.children) serialize(*child, out);
  out += "</" + n.tag + ">";
}

} // namespace

Element Element::create(const char *tag) {
  auto node = std::make_shared<DomNode>();
  node->tag = tag;
  return Element(std::move(node));
}

void Element::setAttr(const char *name, const std::string &value) { setAttribute(*n_, name, value); }
void Element::removeAttr(const char *name) { removeAttribute(*n_, name); }
void Element::setClassName(const std::string &name) { n_->className = name; }

void Element::setStyleText(const std::string &cssText) {
  n_->styleBase = cssText;
  n_->styleOverrides.clear();
}

void Element::setStyle(const char *property, const std::string &value) {
  auto &overrides = n_->styleOverrides;
  overrides.erase(std::remove_if(overrides.begin(), overrides.end(), [&](const auto &kv) { return kv.first == property; }), overrides.end());
  if (!value.empty()) overrides.emplace_back(property, value);
}

void Element::setText(const std::string &text) {
  n_->children.clear();
  n_->rawHtml.clear();
  n_->text = text;
}

void Element::setHtml(const std::string &html) {
  n_->children.clear();
  n_->text.clear();
  n_->rawHtml = html;
}

void Element::clear() {
  for (auto &child : n_->children) child->parent = nullptr;
  n_->children.clear();
  n_->text.clear();
  n_->rawHtml.clear();
}

void Element::setValue(const std::string &value) {
  if (n_->tag == "textarea") setText(value);
  else setAttribute(*n_, "value", value);
}

std::string Element::value() const {
  if (n_->tag == "textarea") return n_->text;
  const std::string *value = findAttribute(*n_, "value");
  return value ? *value : std::string();
}

void Element::setSelectedIndex(int index) {
  int i = 0;
  for (auto &child : n_->children) {
    if (child->tag != "option") continue;
    if (i++ == index) setAttribute(*child, "selected", "");
    else removeAttribute(*child, "selected");
  }
}

void Element::setBool(const char *property, bool on) {
  const std::string name = lower(property);
  if (on) setAttribute(*n_, name, "");
  else removeAttribute(*n_, name);
}

void Element::setString(const char *property, const std::string &value) { setAttribute(*n_, lower(property), value); }
void Element::setNumber(const char *property, int value) { setAttribute(*n_, lower(property), std::to_string(value)); }

void Element::append(const Element &child) {
  detach(*child.n_);
  child.n_->parent = n_.get();
  n_->children.push_back(child.n_);
}

void Element::insertBefore(const Element &child, const Element &reference) {
  if (child.n_ == reference.n_) return;
  detach(*child.n_);
  child.n_->parent = n_.get();
  auto position = n_->children.end();
  if (reference.n_) position = std::find(n_->children.begin(), n_->children.end(), reference.n_);
  n_->children.insert(position, child.n_);
}

void Element::remove() { detach(*n_); }
bool Element::hasParent() const { return n_->parent != nullptr; }
std::string Element::tag() const { return n_->tag; }

void Element::replaceWith(const Element &other) {
  if (!n_->parent || n_ == other.n_) return;
  DomNode *parent = n_->parent;
  detach(*other.n_);
  auto position = std::find_if(parent->children.begin(), parent->children.end(), [&](const auto &c) { return c == n_; });
  other.n_->parent = parent;
  *position = other.n_;
  n_->parent = nullptr;
}

double Element::imageKey() const { return n_->imageKey; }
void Element::setImageKey(double key) { n_->imageKey = key; }

std::string Element::html() const {
  std::string out;
  serialize(*n_, out);
  return out;
}

} // namespace n8v::detail::html

#endif
