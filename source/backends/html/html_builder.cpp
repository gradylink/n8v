#include "html_builder.hpp"

#include "html_assets.hpp"
#include "html_css.hpp"

#include "core/entry_lines.hpp"
#include "core/icon_loader.hpp"
#include "core/image_loader.hpp"
#include "core/markdown/markdown.hpp"
#include "core/markdown/markdown_to_html.hpp"
#include "core/style.hpp"
#include "core/theme_color.hpp"
#include "core/tree/props.hpp"
#include "core/widgets/page_badge.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

namespace n8v::detail::html {
namespace {

namespace css = html_css;

constexpr int kDefaultIconSize = 16;

n8v_sizing fitSizing() { return n8v_sizing_fit(0.0f, 0.0f); }
n8v_sizing growSizing() { return n8v_sizing_grow(0.0f, 0.0f); }

n8v_sizing sizingFor(const NativeNode &nn, bool horizontal) {
  return std::visit(
    Overloaded{
      [&](const n8v_flex_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_panel_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_floating_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_document_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_icon_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_image_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_video_options &o) {
        n8v_sizing s = horizontal ? o.width : o.height;
        if (s.mode == N8V_SIZING_FIT && s.min <= 0.0f && s.max <= 0.0f) return n8v_sizing_fixed(horizontal ? 480.0f : 270.0f);
        return s;
      },
      [&](const n8v_audio_options &o) {
        if (!horizontal) return fitSizing();
        if (o.width.mode == N8V_SIZING_FIT && o.width.min <= 0.0f && o.width.max <= 0.0f) return n8v_sizing_fixed(320.0f);
        return o.width;
      },
      [&](const n8v_divider_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_sidebar_options &) { return growSizing(); },
      [&](const n8v_entry_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_dropdown_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_slider_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const auto &) { return fitSizing(); },
    },
    nn.node->current.props
  );
}

ButtonStyle toButtonStyle(n8v_button_style s) {
  switch (s) {
  case N8V_BUTTON_STYLE_SECONDARY:
    return ButtonStyle::Secondary;
  case N8V_BUTTON_STYLE_GHOST:
    return ButtonStyle::Ghost;
  case N8V_BUTTON_STYLE_PRIMARY:
    break;
  }
  return ButtonStyle::Primary;
}

const char *buttonStyleName(ButtonStyle style) {
  switch (style) {
  case ButtonStyle::Secondary:
    return "secondary";
  case ButtonStyle::Ghost:
    return "ghost";
  case ButtonStyle::Primary:
    break;
  }
  return "primary";
}

std::string semanticClasses(const NativeNode &nn) {
  return std::visit(
    Overloaded{
      [](const std::monostate &) { return std::string(); },
      [](const n8v_flex_options &) { return std::string("n8v-flex"); },
      [](const n8v_panel_options &o) { return std::string("n8v-panel n8v-panel--") + (o.role == N8V_PANEL_ROLE_LIST_ITEM ? "list-item" : "card"); },
      [](const n8v_floating_options &) { return std::string("n8v-floating"); },
      [](const n8v_sidebar_options &o) { return std::string("n8v-sidebar") + (o.collapsed && *o.collapsed ? " n8v-sidebar--collapsed" : ""); },
      [](const n8v_page_options &) { return std::string("n8v-page"); },
      [](const n8v_button_options &o) { return std::string("n8v-button n8v-button--") + buttonStyleName(toButtonStyle(o.style)); },
      [](const n8v_text_options &o) {
        std::string out = "n8v-text";
        if (o.bold) out += " n8v-text--bold";
        if (o.italic) out += " n8v-text--italic";
        if (o.strikethrough) out += " n8v-text--strikethrough";
        if (o.small) out += " n8v-text--small";
        if (o.large) out += " n8v-text--large";
        if (o.darken) out += " n8v-text--darken";
        if ((o.url && *o.url) || o.on_click) out += " n8v-link";
        return out;
      },
      [](const n8v_checkbox_options &) { return std::string("n8v-checkbox"); },
      [](const n8v_toggle_options &) { return std::string("n8v-toggle"); },
      [](const n8v_radio_options &) { return std::string("n8v-radio"); },
      [](const n8v_entry_options &o) { return std::string("n8v-entry") + (o.multiline ? " n8v-entry--multiline" : "") + (o.password ? " n8v-entry--password" : ""); },
      [](const n8v_dropdown_options &) { return std::string("n8v-dropdown"); },
      [](const n8v_slider_options &) { return std::string("n8v-slider"); },
      [](const n8v_image_options &) { return std::string("n8v-image"); },
      [](const n8v_icon_options &) { return std::string("n8v-icon"); },
      [](const n8v_document_options &) { return std::string("n8v-document"); },
      [](const n8v_sidebar_section_options &) { return std::string("n8v-sidebar-section"); },
      [](const n8v_sidebar_divider_options &) { return std::string("n8v-sidebar-divider"); },
      [](const n8v_video_options &) { return std::string("n8v-video"); },
      [](const n8v_audio_options &) { return std::string("n8v-audio"); },
      [](const n8v_divider_options &) { return std::string("n8v-divider"); },
    },
    nn.node->current.props
  );
}

const char *alignItems(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_CENTER:
    return "center";
  case N8V_ALIGN_END:
    return "flex-end";
  case N8V_ALIGN_START:
    break;
  }
  return "flex-start";
}

void attachFraction(n8v_attach_point p, double &fx, double &fy) {
  switch (p) {
  case N8V_ATTACH_POINT_LEFT_TOP:
    fx = 0.0, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_LEFT_CENTER:
    fx = 0.0, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_LEFT_BOTTOM:
    fx = 0.0, fy = 1.0;
    break;
  case N8V_ATTACH_POINT_CENTER_TOP:
    fx = 0.5, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_CENTER_CENTER:
    fx = 0.5, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_CENTER_BOTTOM:
    fx = 0.5, fy = 1.0;
    break;
  case N8V_ATTACH_POINT_RIGHT_TOP:
    fx = 1.0, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_RIGHT_CENTER:
    fx = 1.0, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_RIGHT_BOTTOM:
    fx = 1.0, fy = 1.0;
    break;
  }
}

std::string radioGroupName(const PNode &radio, const int *selected) {
  const PNode *parent = radio.parent;
  int group = 0;
  if (parent) {
    std::vector<const int *> seen;
    for (const auto &sibling : parent->children) {
      const auto *o = propsOf<n8v_radio_options>(*sibling);
      if (!o) continue;
      if (std::find(seen.begin(), seen.end(), o->selected) == seen.end()) seen.push_back(o->selected);
    }
    group = (int)(std::find(seen.begin(), seen.end(), selected) - seen.begin());
  }
  return "n8v-radio-" + std::to_string(parent ? parent->ordinal : 0) + "-" + std::to_string(group);
}

std::string pct(double fraction) { return css::cssNumber((float)(fraction * 100.0)) + "%"; }

} // namespace

void HtmlBuilder::create(PNode &node) {
  auto *nn = new NativeNode;
  nn->node = &node;
  node.handle = nn;
  live_.insert(&node);
  byOrdinal_[node.ordinal] = &node;
  build(*nn);
  apply(*nn);
  registerId(*nn);
}

void HtmlBuilder::update(PNode &node, const OwnedProps &, unsigned) {
  NativeNode &nn = *native(node);
  apply(nn);
  registerId(nn);
  for (NativeNode *kid : nn.kids) refreshChild(nn, *kid);
  if (nn.kind() == Kind::Sidebar) {
    for (NativeNode *kid : nn.kids)
      if (kid->kind() == Kind::Page) apply(*kid);
  }
}

void HtmlBuilder::insert(PNode &parent, PNode &child, size_t index) {
  NativeNode &p = nativeOrRoot(parent);
  NativeNode &c = *native(child);
  p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
  attach(p, c, index);
  refreshChild(p, c);
  afterKidsChanged(p);
}

void HtmlBuilder::move(PNode &parent, PNode &child, size_t index) {
  NativeNode &p = nativeOrRoot(parent);
  NativeNode &c = *native(child);
  const bool hadFocus = !c.widget.isNull() && hasFocus(c.widget);
  p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
  p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
  attach(p, c, index);
  if (hadFocus) focus(c.widget);
  afterKidsChanged(p);
}

void HtmlBuilder::remove(PNode &parent, PNode &child) {
  NativeNode &p = nativeOrRoot(parent);
  NativeNode &c = *native(child);
  c.outer.remove();
  if (c.kind() == Kind::Page) c.content.remove();
  p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
  afterKidsChanged(p);
}

void HtmlBuilder::destroy(PNode &node) {
  NativeNode *nn = native(node);
  if (!nn) return;
  unregisterId(*nn);
  live_.erase(&node);
  byOrdinal_.erase(node.ordinal);
  delete nn;
  node.handle = nullptr;
}

std::string HtmlBuilder::containerCss(NativeNode &nn) {
  std::string out;
  bool hasFloatingKid = false;
  for (NativeNode *kid : nn.kids)
    if (kid->floating) hasFloatingKid = true;

  auto flexBox = [&](n8v_direction direction, uint16_t gap, const n8v_padding *padding, n8v_align hAlign, n8v_align vAlign, bool clipH, bool clipV) {
    const bool row = direction == N8V_DIRECTION_HORIZONTAL;
    out += std::string("display:flex;flex-direction:") + (row ? "row" : "column") + ";gap:" + std::to_string(gap) + "px;";
    if (padding)
      out += "padding:" + std::to_string(padding->top) + "px " + std::to_string(padding->right) + "px " + std::to_string(padding->bottom) + "px " +
             std::to_string(padding->left) + "px;";
    out += std::string("align-items:") + alignItems(row ? vAlign : hAlign) + ";";
    const n8v_align mainAlign = row ? hAlign : vAlign;
    out += std::string("justify-content:") + alignItems(mainAlign) + ";";
    if (clipH || clipV) {
      out += std::string("overflow-x:") + (clipH ? "auto" : "hidden") + ";overflow-y:" + (clipV ? "auto" : "hidden") + ";min-width:0;min-height:0;";
    }
  };
  std::visit(
    Overloaded{
      [&](const n8v_flex_options &o) { flexBox(o.direction, o.gap, &o.padding, o.h_align, o.v_align, o.clip_horizontal, o.clip_vertical); },
      [&](const n8v_panel_options &o) {
        flexBox(o.direction, o.gap, nullptr, o.h_align, o.v_align, o.clip_horizontal, o.clip_vertical);
        const n8v_color accent = resolveThemeColor(o.accent);
        if (accent.a > 0.0f) {
          char bar[64];
          std::snprintf(bar, sizeof(bar), "border-left:4px solid rgb(%d,%d,%d);", (int)accent.r, (int)accent.g, (int)accent.b);
          out += bar;
        }
      },
      [&](const n8v_floating_options &o) {
        out += "display:flex;flex-direction:column;align-items:flex-start;";
        if (o.opaque) {
          out += "background:" + css::cssColor(activePaint().windowBackground()) +
                 ";border-radius:" + css::cssRadius(activePaint().panel(PanelRole::Card, false, false).cornerRadius) + ";";
        }
      },
      [&](const n8v_sidebar_options &) { out += "display:flex;flex-direction:row;min-width:0;min-height:0;"; },
      [](const auto &) {},
    },
    nn.node->current.props
  );
  if (hasFloatingKid) out += "position:relative;";
  return out;
}

std::string HtmlBuilder::sizingCss(NativeNode &nn) {
  if (nn.floating) return {};
  std::string out;
  for (bool horizontal : {true, false}) {
    const n8v_sizing s = sizingFor(nn, horizontal);
    const bool main = nn.parentIsRow == horizontal;
    const std::string dim = horizontal ? "width" : "height";
    switch (s.mode) {
    case N8V_SIZING_FIT:
      break;
    case N8V_SIZING_GROW:
      if (main) out += "flex:1 1 0%;min-" + dim + ":0;";
      else out += "align-self:stretch;";
      break;
    case N8V_SIZING_FIXED:
      out += dim + ":" + css::cssNumber(s.value) + "px;";
      if (main) out += "flex:0 0 auto;";
      break;
    case N8V_SIZING_PERCENT:
      if (main) out += "flex:0 0 " + css::cssNumber(s.value * 100.0f) + "%;";
      else out += dim + ":" + css::cssNumber(s.value * 100.0f) + "%;";
      break;
    }
    if (s.mode != N8V_SIZING_FIXED) {
      if (s.min > 0.0f) out += "min-" + dim + ":" + css::cssNumber(s.min) + "px;";
      if (s.max > 0.0f) out += "max-" + dim + ":" + css::cssNumber(s.max) + "px;";
    }
  }
  return out;
}

std::string HtmlBuilder::floatingCss(NativeNode &nn) {
  const auto *o = propsOf<n8v_floating_options>(*nn.node);
  if (!o) return {};
  double pfx = 0, pfy = 0, efx = 0, efy = 0;
  attachFraction(o->parent_attach_point, pfx, pfy);
  attachFraction(o->element_attach_point, efx, efy);
  std::string out = std::string("position:absolute;z-index:") + (o->keep_on_screen ? "100" : "10") + ";left:calc(" + pct(pfx) + " + " + css::cssNumber(o->offset_x) +
                    "px);top:calc(" + pct(pfy) + " + " + css::cssNumber(o->offset_y) + "px);" + "transform:translate(" + pct(-efx) + "," + pct(-efy) + ");";
  auto dim = [&](const n8v_sizing &s, const char *name) {
    if (s.mode == N8V_SIZING_FIXED) out += std::string(name) + ":" + css::cssNumber(s.value) + "px;";
    else if (s.mode == N8V_SIZING_GROW) out += std::string(name) + ":100%;";
  };
  dim(o->width, "width");
  dim(o->height, "height");
  return out;
}

void HtmlBuilder::applyLayout(NativeNode &nn) {
  if (!nn.node || nn.outer.isNull()) return;
  if (nn.kind() == Kind::Page || isSidebarDecoration(nn.kind())) return;
  std::string css = containerCss(nn) + sizingCss(nn) + floatingCss(nn);
  if (const auto *slider = propsOf<n8v_slider_options>(*nn.node)) {
    float value = slider->min;
    if (const float *bound = std::get_if<float>(&nn.node->bound)) value = *bound;
    const float span = slider->max - slider->min;
    css += "--n8v-p:" + css::cssNumber(span > 0.0f ? (value - slider->min) / span * 100.0f : 0.0f) + "%;";
  }
  setCss(nn.outer, css);
}

void HtmlBuilder::layoutPageContent(NativeNode &page) {
  setCss(
    page.content,
    std::string("flex:1 1 0%;min-width:0;min-height:0;display:") + (page.kids.empty() ? "none" : "flex") + ";flex-direction:column;align-items:flex-start;"
  );
}

void HtmlBuilder::refreshChild(NativeNode &parent, NativeNode &child) {
  bool row = false;
  if (parent.node) {
    if (const auto *flex = propsOf<n8v_flex_options>(*parent.node)) row = flex->direction == N8V_DIRECTION_HORIZONTAL;
    else if (const auto *panel = propsOf<n8v_panel_options>(*parent.node)) row = panel->direction == N8V_DIRECTION_HORIZONTAL;
  }
  child.parentIsRow = row;
  applyLayout(child);
}

size_t HtmlBuilder::pageIndex(NativeNode &sidebar, NativeNode &page) const {
  size_t index = 0;
  for (NativeNode *kid : sidebar.kids) {
    if (kid == &page) break;
    if (kid->kind() == Kind::Page) ++index;
  }
  return index;
}

void HtmlBuilder::attach(NativeNode &parent, NativeNode &child, size_t index) {
  if (parent.kind() == Kind::Sidebar && (child.kind() == Kind::Page || isSidebarDecoration(child.kind()))) {
    Element reference = parent.collapseBottom;
    for (size_t i = index + 1; i < parent.kids.size(); ++i) {
      if (parent.kids[i]->kind() == Kind::Page || isSidebarDecoration(parent.kids[i]->kind())) {
        reference = parent.kids[i]->outer;
        break;
      }
    }
    parent.listEl.insertBefore(child.outer, reference);
    if (child.kind() == Kind::Page) parent.contentArea.append(child.content);
    return;
  }
  Element container = parent.kind() == Kind::Sidebar ? parent.contentArea : parent.content;
  Element reference = index + 1 < parent.kids.size() ? parent.kids[index + 1]->outer : Element();
  container.insertBefore(child.outer, reference);
}

void HtmlBuilder::afterKidsChanged(NativeNode &container) {
  if (container.kind() == Kind::Sidebar) {
    for (NativeNode *kid : container.kids)
      if (kid->kind() == Kind::Page) layoutPageContent(*kid);
  } else if (container.kind() == Kind::Page) {
    layoutPageContent(container);
  } else {
    applyLayout(container);
  }
}

void HtmlBuilder::unregisterId(NativeNode &nn) {
  if (nn.registeredId.empty()) return;
  auto it = byId_.find(nn.registeredId);
  if (it != byId_.end() && it->second == &nn) byId_.erase(it);
  hovered_.erase(nn.registeredId);
  nn.registeredId.clear();
}

void HtmlBuilder::registerId(NativeNode &nn) {
  const char *id = tree::idOf(nn.node->current.props);
  const std::string wanted = id ? id : "";
  if (wanted != nn.registeredId) {
    unregisterId(nn);
    if (!wanted.empty()) {
      byId_[wanted] = &nn;
      nn.registeredId = wanted;
    }
  }
  const auto *flex = propsOf<n8v_flex_options>(*nn.node);
  if (!nn.outer.isNull() && nn.kind() != Kind::Page && (!wanted.empty() || (flex && flex->on_hover))) {
    setAttr(nn.outer, "data-n8v", std::to_string(nn.node->ordinal));
    setAttr(nn.outer, "data-n8v-hover", std::to_string(nn.node->ordinal));
  }
}

void HtmlBuilder::build(NativeNode &nn) {
  switch (nn.kind()) {
  case Kind::Flex:
  case Kind::Panel:
  case Kind::Floating:
    nn.outer = nn.content = Element::create("div");
    nn.floating = nn.kind() == Kind::Floating;
    break;
  case Kind::Text: {
    const auto *text = propsOf<n8v_text_options>(*nn.node);
    nn.outer = nn.widget = Element::create(text && text->url && *text->url ? "a" : "span");
    break;
  }
  case Kind::Button: {
    nn.outer = nn.widget = Element::create("button");
    nn.outer.setAttr("type", "button");
    nn.label = Element::create("span");
    nn.image = Element::create("img");
    nn.outer.append(nn.label);
    break;
  }
  case Kind::Checkbox:
  case Kind::Toggle:
  case Kind::Radio: {
    nn.outer = Element::create("label");
    nn.widget = Element::create("input");
    nn.widget.setString("type", std::string(nn.kind() == Kind::Radio ? "radio" : "checkbox"));
    if (nn.kind() == Kind::Toggle) setAttr(nn.widget, "role", "switch");
    nn.label = Element::create("span");
    nn.outer.append(nn.widget);
    nn.outer.append(nn.label);
    break;
  }
  case Kind::Entry: {
    const auto *entry = propsOf<n8v_entry_options>(*nn.node);
    if (entry && entry->multiline) {
      nn.outer = nn.widget = Element::create("textarea");
    } else {
      nn.outer = nn.widget = Element::create("input");
      nn.widget.setString("type", std::string("text"));
    }
    break;
  }
  case Kind::Dropdown:
    nn.outer = nn.widget = Element::create("select");
    break;
  case Kind::Slider: {
    nn.outer = nn.widget = Element::create("input");
    nn.widget.setString("type", std::string("range"));
    nn.widget.setString("step", std::string("any"));
    break;
  }
  case Kind::Image:
  case Kind::Icon:
    nn.outer = nn.widget = Element::create("img");
    break;
  case Kind::Divider:
    nn.outer = nn.widget = Element::create("hr");
    break;
  case Kind::Audio:
    nn.outer = nn.widget = Element::create("audio");
    break;
  case Kind::Video:
    nn.outer = nn.widget = Element::create("video");
    nn.widget.setBool("playsInline", true);
    break;
  case Kind::Document:
    nn.outer = nn.widget = Element::create("div");
    break;
  case Kind::Sidebar: {
    nn.outer = Element::create("div");
    nn.listEl = Element::create("nav");
    nn.titleEl = Element::create("div");
    nn.titleText = Element::create("span");
    nn.collapseTop = Element::create("button");
    nn.collapseBottom = Element::create("button");
    nn.titleEl.append(nn.titleText);
    nn.titleEl.append(nn.collapseTop);
    nn.contentArea = Element::create("div");
    nn.listEl.append(nn.titleEl);
    nn.listEl.append(nn.collapseBottom);
    nn.outer.append(nn.listEl);
    nn.outer.append(nn.contentArea);
    nn.content = nn.contentArea;
    break;
  }
  case Kind::SidebarSection: {
    nn.outer = nn.widget = Element::create("div");
    break;
  }
  case Kind::SidebarDivider: {
    nn.outer = nn.widget = Element::create("div");
    break;
  }
  case Kind::Page: {
    nn.outer = Element::create("button");
    nn.outer.setAttr("type", "button");
    nn.widget = nn.outer;
    nn.label = Element::create("span");
    nn.image = Element::create("img");
    nn.outer.append(nn.image);
    nn.outer.append(nn.label);
    nn.content = Element::create("div");
    break;
  }
  case Kind::Root:
    break;
  }
  switch (nn.kind()) {
  case Kind::Button:
    setPartClass(nn.label, "n8v-button__label");
    setPartClass(nn.image, "n8v-button__icon");
    break;
  case Kind::Checkbox:
  case Kind::Toggle:
  case Kind::Radio: {
    const char *kind = nn.kind() == Kind::Radio ? "radio" : nn.kind() == Kind::Toggle ? "toggle" : "checkbox";
    setPartClass(nn.widget, (std::string("n8v-") + kind + "__input").c_str());
    setPartClass(nn.label, (std::string("n8v-") + kind + "__label").c_str());
    break;
  }
  case Kind::Page:
    setPartClass(nn.label, "n8v-page__label");
    setPartClass(nn.image, "n8v-page__icon");
    setPartClass(nn.content, "n8v-page__content");
    break;
  case Kind::Sidebar:
    setPartClass(nn.titleEl, "n8v-sidebar__header");
    setPartClass(nn.titleText, "n8v-sidebar__title");
    setPartClass(nn.collapseTop, "n8v-sidebar__collapse n8v-sidebar__collapse--top");
    setPartClass(nn.collapseBottom, "n8v-sidebar__collapse n8v-sidebar__collapse--bottom");
    setPartClass(nn.contentArea, "n8v-sidebar__content");
    break;
  default:
    break;
  }
}

void HtmlBuilder::applyButton(NativeNode &nn, const n8v_button_options &o) {
  const Paint &paint = activePaint();
  ButtonStyle style = toButtonStyle(o.style);
  if (o.checked) {
    const bool on = *o.checked;
    nn.outer.setAttr("aria-pressed", on ? "true" : "false");
    if (on && style == ButtonStyle::Ghost) style = ButtonStyle::Secondary;
    else if (on && style == ButtonStyle::Secondary) style = ButtonStyle::Primary;
  }
  setClass(nn.outer, classFor(css::buttonCss(paint, style)));
  bindEvents(nn, nn.outer, true);
  nn.label.setText(nn.node->current.label);
  const bool hasIcon = o.icon && *o.icon;
  const bool hasImage = !hasIcon && o.image && *o.image;
  if (hasIcon || hasImage) {
    const ButtonPaint bp = paint.button(style, false, false);
    if (hasIcon) {
      setImage(nn.image, getOrDecodeIcon(o.icon, bp.fontSize, bp.textColor));
    } else {
      n8v_image_options imageOptions = {};
      imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
      imageOptions.path = o.image;
      const DecodedImage *decoded = getOrDecodeImage(imageOptions);
      setImage(nn.image, decoded, o.image);
      const int height = o.image_height > 0 ? o.image_height : (int)(bp.fontSize * 1.15f);
      nn.image.setStyle("height", std::to_string(height) + "px");
      nn.image.setStyle("width", "auto");
    }
    if (o.icon_position == N8V_ICON_POSITION_TRAILING) nn.outer.append(nn.image);
    else nn.outer.insertBefore(nn.image, nn.label);
  } else if (nn.image.hasParent()) {
    nn.image.remove();
  }
  nn.label.setBool("hidden", nn.node->current.label.empty());
}

void HtmlBuilder::applyPage(NativeNode &nn, const n8v_page_options &o) {
  const Paint &paint = activePaint();
  const NativeNode *sidebar = nn.node->parent ? native(*nn.node->parent) : nullptr;
  int selected = 0;
  if (sidebar) {
    if (const int *bound = std::get_if<int>(&sidebar->node->bound)) selected = *bound;
  }
  const bool isSelected = sidebar && (int)pageIndex(*const_cast<NativeNode *>(sidebar), nn) == selected;
  const ButtonStyle style = isSelected ? ButtonStyle::Primary : ButtonStyle::Secondary;
  bool collapsed = false;
  if (sidebar && sidebar->node) {
    if (const auto *so = propsOf<n8v_sidebar_options>(*sidebar->node)) collapsed = so->collapsed && *so->collapsed;
  }
  semantic_ = std::string("n8v-page") + (isSelected ? " n8v-page--selected" : "");
  setClass(nn.outer, classFor(css::sidebarRowCss(paint, isSelected, collapsed)));
  if (sidebar && !collapsed) {
    bool anyImage = false;
    for (const NativeNode *kid : sidebar->kids) {
      const auto *po = kid->node ? propsOf<n8v_page_options>(*kid->node) : nullptr;
      if (po && !(po->icon && *po->icon) && po->image && *po->image) anyImage = true;
    }
    const SidebarPaint rows = paint.sidebar();
    Padding pad = rows.rowPadding;
    if (!(pad.left || pad.right || pad.top || pad.bottom)) pad = paint.button(ButtonStyle::Secondary, false, false).padding;
    if (anyImage) nn.outer.setStyle("min-height", std::to_string((int)(paint.button(style, false, false).fontSize * 2 + pad.top + pad.bottom)) + "px");
    else nn.outer.setStyle("min-height", std::string());
  }
  bindEvents(nn, nn.outer, true);
  const std::string name = o.name ? o.name : "";
  nn.label.setText(pageLabelWithBadges(name, o.notify, o.badge));
  nn.label.setBool("hidden", collapsed);
  if (collapsed) setAttr(nn.outer, "title", name);
  else nn.outer.removeAttr("title");
  const bool hasIcon = o.icon && *o.icon;
  const bool hasImage = !hasIcon && o.image && *o.image;
  const ButtonPaint bp = paint.button(style, false, false);
  if (hasIcon) {
    const int glyph = collapsed ? 24 : bp.fontSize;
    setImage(nn.image, getOrDecodeIcon(o.icon, glyph, bp.textColor));
    nn.image.setStyle("width", std::to_string(glyph) + "px");
    nn.image.setStyle("height", std::to_string(glyph) + "px");
    nn.image.setStyle("border-radius", std::string());
  } else if (hasImage) {
    n8v_image_options imageOptions = {};
    imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
    imageOptions.path = o.image;
    const int box = collapsed ? 40 : bp.fontSize * 2;
    setImage(nn.image, getOrFitImageInSquare(getOrDecodeImage(imageOptions), box));
    nn.image.setStyle("width", std::to_string(box) + "px");
    nn.image.setStyle("height", std::to_string(box) + "px");
    nn.image.setStyle("border-radius", std::string("22%"));
  }
  nn.image.setBool("hidden", !(hasIcon || hasImage));
  layoutPageContent(nn);
}

std::string HtmlBuilder::multilineEntryCss(NativeNode &nn, const n8v_entry_options &o, const Paint &paint) {
  const EntryLines lines = entryLines(o);
  const EntryPaint idle = paint.entry(false, false);
  const std::string chrome = css::cssNumber((float)(idle.padding.top + idle.padding.bottom) + 2.0f * idle.borderWidth) + "px";
  auto heightFor = [&](int count) { return "calc(" + std::to_string(count) + " * 1lh + " + chrome + ")"; };

  setAttr(nn.widget, "data-n8v-submit", o.submit_mode == N8V_SUBMIT_ON_CTRL_ENTER ? "ctrl" : "enter");
  std::string extra = "&{resize:none;overflow-y:auto;display:block;";
  if (lines.fixed) {
    extra += "height:" + heightFor(lines.min) + ";}\n";
    nn.widget.removeAttr("data-n8v-grow");
  } else {
    extra += "height:auto;field-sizing:content;min-height:" + heightFor(lines.min) + ";max-height:" + heightFor(lines.max) + ";}\n";
    setAttr(nn.widget, "data-n8v-grow", "1");
  }
  return extra;
}

void HtmlBuilder::apply(NativeNode &nn) {
  PNode &pn = *nn.node;
  const Paint &paint = activePaint();
  const std::string &label = pn.current.label;
  semantic_ = semanticClasses(nn);

  std::visit(
    Overloaded{
      [&](const std::monostate &) {},
      [&](const n8v_flex_options &o) {
        nn.stickyBottom = o.stick_to_bottom && o.clip_vertical;
        setClass(nn.outer);
      },
      [&](const n8v_panel_options &o) {
        std::string css = css::panelCss(paint, (o.role == N8V_PANEL_ROLE_LIST_ITEM ? PanelRole::ListItem : PanelRole::Card));
        if (o.has_padding)
          css += "&{padding:" + std::to_string(o.padding.top) + "px " + std::to_string(o.padding.right) + "px " + std::to_string(o.padding.bottom) + "px " +
                 std::to_string(o.padding.left) + "px}\n";
        setClass(nn.outer, classFor(css));
      },
      [&](const n8v_floating_options &) { setClass(nn.outer); },
      [&](const n8v_sidebar_options &o) {
        const bool collapsible = o.collapsed != nullptr;
        const bool collapsed = collapsible && *o.collapsed;
        float listWidth = o.width.mode == N8V_SIZING_FIXED ? o.width.value : 0.0f;
        std::string listCss = "display:flex;flex-direction:column;align-items:stretch;flex:none;min-height:0;";
        if (collapsed) {
          listCss += "width:72px;min-width:72px;max-width:72px;";
        } else {
          if (listWidth > 0.0f) listCss += "width:" + css::cssNumber(listWidth) + "px;";
          if (o.min_width > 0.0f) listCss += "min-width:" + css::cssNumber(o.min_width) + "px;";
          if (o.max_width > 0.0f) listCss += "max-width:" + css::cssNumber(o.max_width) + "px;";
        }
        setCss(nn.listEl, listCss);
        setClass(nn.outer);
        nn.listEl.setClassName(std::string("n8v-sidebar__list ") + classFor(css::sidebarListCss(paint)));
        setCss(nn.contentArea, "flex:1 1 0%;min-width:0;min-height:0;display:flex;flex-direction:column;align-items:stretch;position:relative;");
        const std::string title = o.title ? o.title : "";
        TextOptions titleOptions;
        titleOptions.bold = true;
        const TextPaint tp = paint.text(titleOptions);
        nn.titleText.setText(title);
        setCss(
          nn.titleEl,
          std::string("display:") + ((title.empty() && !collapsible) || collapsed ? "none" : "flex") +
            ";position:relative;align-items:center;justify-content:center;font-weight:bold;color:" + css::cssColor(tp.color) + ";font-family:" + css::cssFontFamily(tp.font) +
            ";font-size:" + css::cssNumber(tp.fontSize) + "px;padding:6px 36px 12px;flex:none;"
        );
        const std::string arrowCss = "appearance:none;-webkit-appearance:none;border:0;background-color:transparent;cursor:pointer;width:28px;height:28px;flex:none;padding:0;"
                                     "background-repeat:no-repeat;background-position:center;background-size:16px 16px;opacity:.75;border-radius:6px;";
        setCss(
          nn.collapseTop,
          arrowCss + css::chevronCss(tp.color, false) +
            "position:absolute;right:6px;top:50%;transform:translateY(-50%);display:" + (collapsible && !collapsed ? "block" : "none") + ";"
        );
        setCss(nn.collapseBottom, arrowCss + css::chevronCss(tp.color, true) + "align-self:center;margin-top:6px;display:" + (collapsed ? "block" : "none") + ";");
        setAttr(nn.collapseTop, "title", "Collapse");
        setAttr(nn.collapseBottom, "title", "Expand");
        bindEvents(nn, nn.collapseTop, true);
        bindEvents(nn, nn.collapseBottom, true);
      },
      [&](const n8v_page_options &o) { applyPage(nn, o); },
      [&](const n8v_button_options &o) { applyButton(nn, o); },
      [&](const n8v_text_options &o) {
        TextOptions options;
        options.bold = o.bold;
        options.italic = o.italic;
        options.strikethrough = o.strikethrough;
        options.small = o.small;
        options.darken = o.darken;
        options.large = o.large;
        options.url = o.url ? o.url : "";
        const n8v_color color = resolveThemeColor(o.color);
        options.color = {color.r, color.g, color.b, color.a};
        if (o.on_click) options.onClick = [] {};
        const bool hasUrl = o.url && *o.url;
        if ((nn.outer.tag() == "a") != hasUrl) {
          Element fresh = Element::create(hasUrl ? "a" : "span");
          nn.outer.replaceWith(fresh);
          nn.outer = nn.widget = fresh;
        }
        setClass(nn.outer, classFor(css::textCss(paint, options)));
        nn.outer.setText(label);
        if (hasUrl) nn.outer.setAttr("href", o.url);
        else nn.outer.removeAttr("href");
        bindEvents(nn, nn.outer, o.on_click != nullptr);
        if (!o.on_click) nn.outer.removeAttr("data-n8v-click");
      },
      [&](const n8v_checkbox_options &) {
        setClass(nn.outer, classFor(css::checkboxCss(paint)));
        bindEvents(nn, nn.widget, false);
        nn.label.setText(label);
        if (const bool *checked = std::get_if<bool>(&pn.bound)) nn.widget.setBool("checked", *checked);
      },
      [&](const n8v_toggle_options &) {
        setClass(nn.outer, classFor(css::toggleCss(paint)));
        bindEvents(nn, nn.widget, false);
        nn.label.setText(label);
        if (const bool *checked = std::get_if<bool>(&pn.bound)) nn.widget.setBool("checked", *checked);
      },
      [&](const n8v_radio_options &o) {
        setClass(nn.outer, classFor(css::radioCss(paint)));
        bindEvents(nn, nn.widget, false);
        nn.label.setText(label);
        nn.widget.setString("name", radioGroupName(pn, o.selected));
        if (const int *selected = std::get_if<int>(&pn.bound)) nn.widget.setBool("checked", *selected == o.value);
      },
      [&](const n8v_entry_options &o) {
        setClass(nn.outer, classFor(css::entryCss(paint) + (o.multiline ? multilineEntryCss(nn, o, paint) : std::string())));
        bindEvents(nn, nn.widget, false);
        if (!o.multiline) nn.widget.setString("type", std::string(o.password ? "password" : "text"));
        nn.widget.setString("placeholder", std::string(o.placeholder ? o.placeholder : ""));
        if (const std::string *text = std::get_if<std::string>(&pn.bound)) {
          if (nn.widget.value() != *text) {
            nn.widget.setValue(*text);
            if (o.multiline) autoGrow(nn.widget);
          }
        }
      },
      [&](const n8v_dropdown_options &o) {
        setClass(nn.outer, classFor(css::dropdownCss(paint)));
        bindEvents(nn, nn.widget, false);
        const bool hasPlaceholder = o.placeholder && *o.placeholder;
        std::string key = std::string(o.placeholder ? o.placeholder : "") + '\n';
        for (size_t i = 0; i < o.item_count; ++i) key += std::string(o.items[i] ? o.items[i] : "") + '\n';
        if (key != nn.lastItemsKey) {
          nn.widget.clear();
          if (hasPlaceholder) {
            Element option = Element::create("option");
            option.setText(std::string(o.placeholder));
            option.setString("value", "-1");
            option.setBool("disabled", true);
            option.setBool("hidden", true);
            nn.widget.append(option);
          }
          for (size_t i = 0; i < o.item_count; ++i) {
            Element option = Element::create("option");
            option.setText(std::string(o.items[i] ? o.items[i] : ""));
            option.setString("value", std::to_string(i));
            nn.widget.append(option);
          }
          nn.lastItemsKey = key;
        }
        const int *selected = std::get_if<int>(&pn.bound);
        const bool inRange = selected && *selected >= 0 && (size_t)*selected < o.item_count;
        nn.widget.setSelectedIndex(inRange ? *selected + (hasPlaceholder ? 1 : 0) : (hasPlaceholder ? 0 : -1));
      },
      [&](const n8v_slider_options &o) {
        setClass(nn.outer, classFor(css::sliderCss(paint)));
        bindEvents(nn, nn.widget, false);
        nn.widget.setString("min", css::cssNumber(o.min));
        nn.widget.setString("max", css::cssNumber(o.max));
        if (const float *value = std::get_if<float>(&pn.bound)) nn.widget.setValue(css::cssNumber(*value));
      },
      [&](const n8v_image_options &o) {
        const DecodedImage *decoded = getOrDecodeImage(o);
        CornerRadius radius;
        switch (o.rounding.mode) {
        case N8V_ROUNDING_STYLE_DEFAULT:
          radius = paint.image().cornerRadius;
          break;
        case N8V_ROUNDING_NONE:
          radius = {};
          break;
        case N8V_ROUNDING_FIXED:
          radius = {o.rounding.radius.top_left, o.rounding.radius.top_right, o.rounding.radius.bottom_left, o.rounding.radius.bottom_right};
          break;
        }
        setClass(nn.outer, classFor(css::imageCss(radius)));
        setImage(nn.widget, decoded, o.source_kind == N8V_IMAGE_SOURCE_PATH ? o.path : nullptr);
        if (decoded) {
          if (o.width.mode == N8V_SIZING_FIT && o.width.min <= 0.0f && o.width.max <= 0.0f) nn.widget.setNumber("width", decoded->width);
          if (o.height.mode == N8V_SIZING_FIT && o.height.min <= 0.0f && o.height.max <= 0.0f) nn.widget.setNumber("height", decoded->height);
        }
      },
      [&](const n8v_video_options &o) {
        CornerRadius radius;
        switch (o.rounding.mode) {
        case N8V_ROUNDING_STYLE_DEFAULT:
          radius = paint.image().cornerRadius;
          break;
        case N8V_ROUNDING_NONE:
          radius = {};
          break;
        case N8V_ROUNDING_FIXED:
          radius = {o.rounding.radius.top_left, o.rounding.radius.top_right, o.rounding.radius.bottom_left, o.rounding.radius.bottom_right};
          break;
        }
        setClass(nn.outer, classFor(css::imageCss(radius)));
        const std::string source = o.source ? o.source : "";
        if (source != nn.videoSource) {
          nn.videoSource = source;
          nn.widget.setString("src", mediaUrl(source));
        }
        nn.widget.setBool("controls", o.controls);
        nn.widget.setBool("loop", o.loop);
        nn.widget.setBool("muted", o.muted);
        nn.widget.setBool("autoplay", o.autoplay);
      },
      [&](const n8v_divider_options &original) {
        n8v_divider_options o = original;
        o.color = resolveThemeColor(o.color);
        std::string css = "&{border:0;margin:0;width:100%;height:0;";
        if (o.color.a > 0.0f) {
          char line[96];
          std::snprintf(line, sizeof(line), "border-top:1px solid rgba(%d,%d,%d,%.3f);}\n", (int)o.color.r, (int)o.color.g, (int)o.color.b, o.color.a / 255.0f);
          css += line;
        } else {
          css += "border-top:1px solid currentColor;opacity:.3;}\n";
        }
        setClass(nn.outer, classFor(css));
      },
      [&](const n8v_audio_options &o) {
        const std::string source = o.source ? o.source : "";
        if (source != nn.videoSource) {
          nn.videoSource = source;
          nn.widget.setString("src", mediaUrl(source));
        }
        setClass(nn.outer);
        nn.widget.setBool("controls", true);
        nn.widget.setBool("loop", o.loop);
        nn.widget.setBool("muted", o.muted);
        nn.widget.setBool("autoplay", o.autoplay);
      },
      [&](const n8v_icon_options &o) {
        const IconPaint ip = paint.icon();
        const float size = o.width.mode == N8V_SIZING_FIXED ? o.width.value : ip.defaultSize;
        const Color tint = o.tint.a > 0.0f ? Color{o.tint.r, o.tint.g, o.tint.b, o.tint.a} : ip.tint;
        setClass(nn.outer, classFor(css::imageCss({})));
        setImage(nn.widget, getOrDecodeIcon(o.name ? o.name : "", (uint16_t)size, tint));
        nn.widget.setNumber("width", (int)size);
        nn.widget.setNumber("height", (int)size);
      },
      [&](const n8v_sidebar_section_options &o) {
        nn.widget.setText(std::string(o.title ? o.title : ""));
        setClass(nn.outer);
        setCss(nn.outer, "flex:none;padding:12px 12px 4px;font-weight:bold;font-size:0.85em;opacity:0.65;text-align:left;");
      },
      [&](const n8v_sidebar_divider_options &) {
        setClass(nn.outer);
        setCss(nn.outer, "flex:none;height:1px;margin:6px 12px;background:currentColor;opacity:0.18;");
      },
      [&](const n8v_document_options &o) {
        setClass(nn.outer, classFor(css::documentCss(paint)));
        const markdown::CachedDocument cached = markdown::getOrParse(o.markdown ? o.markdown : "");
        markdown::HtmlOptions htmlOptions;
        htmlOptions.linkStyler = markdown::makeLinkStyler(o.link_style, o.link_style_userdata);
        htmlOptions.defaultImageHeight = (int)(paint.text({}).fontSize * 1.4f);
        const std::string html = markdown::toHtml(*cached.document, htmlOptions);
        if (nn.lastItemsKey != html) {
          nn.widget.setHtml(html);
          nn.lastItemsKey = html;
        }
      },
    },
    pn.current.props
  );
  applyLayout(nn);
}

std::string HtmlBuilder::baseCss() {
  return "@layer n8v{\n"
         "html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;background:#ffffff}\n"
         "*,*::before,*::after{box-sizing:border-box}\n"
         "#n8v-root{position:relative;display:flex;flex-direction:column;align-items:flex-start;width:100%;height:100%;overflow:hidden}\n"
         "#n8v-root button,#n8v-root input,#n8v-root select{font-family:inherit}\n"
         "#n8v-root img{pointer-events:none}\n"
         "}\n";
}

void HtmlBuilder::setClass(Element &el, const std::string &generated) { el.setClassName(generated.empty() ? semantic_ : semantic_ + " " + generated); }

std::string HtmlBuilder::classFor(const std::string &tmpl) {
  auto it = classes_.find(tmpl);
  if (it != classes_.end()) return it->second;
  uint32_t h = 2166136261u;
  for (unsigned char c : tmpl) h = (h ^ c) * 16777619u;
  char hex[16];
  std::snprintf(hex, sizeof hex, "%08x", h);
  std::string name = std::string("n8v-g-") + hex;
  emitCss("@layer n8v{" + css::expand(tmpl, "." + name) + "}\n");
  classes_.emplace(tmpl, name);
  return name;
}

void HtmlBuilder::installRootStyle() {
  emitCss("@layer n8v{" + css::expand(css::textCss(activePaint(), {}), "#n8v-root") + "}\n");
  const Color window = activePaint().windowBackground();
  const bool dark = (0.2126f * window.r + 0.7152f * window.g + 0.0722f * window.b) < 128.0f;
  cssAppend("@layer n8v{html,body,#n8v-root{background:" + css::cssColor(window) + ";color-scheme:" + (dark ? "dark" : "light") + "}}\n");
}

void HtmlBuilder::emitCss(const std::string &css) {
  for (FontFamily family : {FontFamily::DejaVuSans, FontFamily::Roboto, FontFamily::Inter, FontFamily::Selawik}) {
    if (fontsDeclared_.count(family)) continue;
    std::string name = html_css::cssFontFamily(family);
    name = name.substr(0, name.find(','));
    if (css.find(name) == std::string::npos) continue;
    fontsDeclared_.insert(family);
    cssAppend(html_assets::fontFaceCss(family, [&](const std::string &file, const std::vector<unsigned char> &bytes) { return fontUrl(file, bytes); }));
  }
  cssAppend(css);
}

void HtmlBuilder::restyleAll() {
  styleGeneration_ = activeStyleGeneration();
  classes_.clear();
  fontsDeclared_.clear();
  cssReset();
  installRootStyle();
  for (PNode *pn : live_) apply(*native(*pn));
}

void HtmlBuilder::bindEvents(NativeNode &nn, Element &el, bool click) {
  setAttr(el, "data-n8v", std::to_string(nn.node->ordinal));
  if (click) setAttr(el, "data-n8v-click", "1");
}

void HtmlBuilder::setImage(Element &img, const DecodedImage *decoded, const char *sourcePath) {
  const std::string path = sourcePath ? sourcePath : "";
  double key = decoded ? (double)(uintptr_t)decoded + (double)decoded->width * 3.0 + (double)decoded->height * 7.0 : 0.0;
  if (!decoded && !path.empty()) key = (double)std::hash<std::string>{}(path) + 1.0;
  if (img.imageKey() == key) return;
  img.setImageKey(key);
  std::string url;
  if (!path.empty()) url = imagePathUrl(path, decoded != nullptr);
  if (url.empty() && decoded) url = imageUrl(*decoded);
  if (url.empty()) img.removeAttr("src");
  else img.setString("src", url);
}

} // namespace n8v::detail::html
