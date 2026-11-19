#pragma once

#include "html_dom.hpp"

#include "core/image_loader.hpp"
#include "core/tree/persistent.hpp"

#include <n8v/style.hpp>

#include <n8v/n8v_c.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace n8v::detail::html {

using tree::OwnedProps;
using tree::PNode;

template <typename... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <typename T> const T *propsOf(const PNode &node) { return std::get_if<T>(&node.current.props); }

enum class NativeKind {
  Root,
  Flex,
  Panel,
  Floating,
  Sidebar,
  Page,
  Button,
  Text,
  Checkbox,
  Toggle,
  Radio,
  Entry,
  Dropdown,
  Slider,
  Image,
  Icon,
  Document,
  SidebarSection,
  SidebarDivider,
  Video,
  Audio,
  Divider
};

struct NativeNode {
  std::string videoSource;
  NativeKind kind() const { return node ? (NativeKind)node->current.props.index() : NativeKind::Root; }

  PNode *node = nullptr;
  Element outer;
  Element content;
  Element widget;
  Element label;
  Element image;

  // sidebar
  Element listEl;
  Element titleEl;
  Element titleText;
  Element collapseTop;
  Element collapseBottom;
  Element contentArea;

  bool floating = false;
  bool parentIsRow = false;
  bool stickyBottom = false;
  double lastClientHeight = -1.0;
  bool stickyAtBottom = true;
  double lastScrollHeight = 0.0;
  std::string lastItemsKey;
  std::string registeredId;
  std::vector<NativeNode *> kids;
};

class HtmlBuilder : public tree::TreeOps {
public:
  void create(PNode &node) override;
  void update(PNode &node, const OwnedProps &previous, unsigned flags) override;
  void insert(PNode &parent, PNode &child, size_t index) override;
  void move(PNode &parent, PNode &child, size_t index) override;
  void remove(PNode &parent, PNode &child) override;
  void destroy(PNode &node) override;

  static std::string baseCss();

protected:
  using Kind = NativeKind;

  virtual void cssAppend(const std::string &css) = 0;
  virtual void cssReset() = 0;
  virtual std::string mediaUrl(const std::string &source) { return source; }
  virtual std::string fontUrl(const std::string &fileName, const std::vector<unsigned char> &bytes) = 0;
  virtual std::string imageUrl(const DecodedImage &image) = 0;
  virtual std::string imagePathUrl(const std::string &path, bool decoded) = 0;
  virtual bool hasFocus(const Element &) const { return false; }
  virtual void focus(Element &) {}
  virtual void autoGrow(Element &) {}

  void installRootStyle();
  void restyleAll();

  NativeNode *native(PNode &node) const { return static_cast<NativeNode *>(node.handle); }
  NativeNode &nativeOrRoot(PNode &node) { return node.parent ? *native(node) : root_; }

  void setCss(Element &el, const std::string &cssText) { el.setStyleText(cssText); }
  void setClass(Element &el, const std::string &generated = {});
  static void setPartClass(Element &el, const char *name) { el.setClassName(name); }
  std::string classFor(const std::string &tmpl);
  void setAttr(Element &el, const char *name, const std::string &value) { el.setAttr(name, value); }
  void bindEvents(NativeNode &nn, Element &el, bool click);
  void setImage(Element &img, const DecodedImage *decoded, const char *sourcePath = nullptr);
  void emitCss(const std::string &css);

  void build(NativeNode &nn);
  void apply(NativeNode &nn);
  void applyButton(NativeNode &nn, const n8v_button_options &o);
  std::string multilineEntryCss(NativeNode &nn, const n8v_entry_options &o, const Paint &paint);
  void applyPage(NativeNode &nn, const n8v_page_options &o);
  void applyLayout(NativeNode &nn);
  std::string containerCss(NativeNode &nn);
  std::string sizingCss(NativeNode &nn);
  std::string floatingCss(NativeNode &nn);
  void layoutPageContent(NativeNode &page);
  void refreshChild(NativeNode &parent, NativeNode &child);
  void attach(NativeNode &parent, NativeNode &child, size_t index);
  void afterKidsChanged(NativeNode &container);
  size_t pageIndex(NativeNode &sidebar, NativeNode &page) const;

  static bool isSidebarDecoration(NativeKind kind) { return kind == NativeKind::SidebarSection || kind == NativeKind::SidebarDivider; }
  void registerId(NativeNode &nn);
  void unregisterId(NativeNode &nn);

  NativeNode root_;
  std::unordered_set<PNode *> live_;
  std::unordered_map<int, PNode *> byOrdinal_;
  std::unordered_map<std::string, NativeNode *> byId_;
  std::unordered_set<std::string> hovered_;
  std::unordered_map<std::string, std::string> classes_;
  std::unordered_set<FontFamily> fontsDeclared_;
  std::string semantic_;
  uint64_t styleGeneration_ = 0;
};

} // namespace n8v::detail::html
