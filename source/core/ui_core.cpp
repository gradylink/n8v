#include <n8v/n8v_c.h>

#include "core/backend.hpp"

#include "core/clay_convert.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using namespace n8v::detail::ui_internal;

bool initialized = false;
std::vector<char> clayMemory;

Clay_Dimensions measureText(Clay_StringSlice text, Clay_TextElementConfig *config, void * /*userData*/) {
  auto *flags = static_cast<n8v::detail::TextStyleFlags *>(config->userData);
  n8v::FontFamily family = flags ? flags->font : n8v::FontFamily::DejaVuSans;
  bool bold = flags && flags->bold;
  bool italic = flags && flags->italic;
  return n8v::activeBackend().measureText(std::string_view(text.chars, (size_t)text.length), family, config->fontSize, bold, italic);
}

void clayErrorHandler(Clay_ErrorData errorData) { std::fprintf(stderr, "[n8v] Clay error: %.*s\n", (int)errorData.errorText.length, errorData.errorText.chars); }

Clay_String toClayStr(const std::string &s) { return Clay_String{false, (int32_t)s.size(), s.data()}; }

struct PendingScrollTarget {
  std::string containerId;
  enum class Kind { Bottom, Top, Left, Right, Offset, Element } kind;
  std::string elementId;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float paddingLeft = 0.0f;
  float paddingTop = 0.0f;
  int framesWaited = 0;
};
std::vector<PendingScrollTarget> pendingScrollTargets;
std::vector<std::string> stickyBottomContainerIds;
constexpr float kStickyBottomEpsilonPx = 24.0f;
constexpr int kScrollRequestTimeoutFrames = 60;

void applyScrollY(n8v::Backend &backend, uint32_t containerId, float y, const Clay_ScrollContainerData &scrollData) {
  if (backend.ownsScrollMath()) {
    scrollData.scrollPosition->y = -y;
  } else {
    backend.setScrollOffsetY(containerId, y);
  }
}
void applyScrollX(n8v::Backend &backend, uint32_t containerId, float x, const Clay_ScrollContainerData &scrollData) {
  if (backend.ownsScrollMath()) {
    scrollData.scrollPosition->x = -x;
  } else {
    backend.setScrollOffsetX(containerId, x);
  }
}

void resolvePendingScrollRequests() {
  n8v::Backend &backend = n8v::activeBackend();

  for (const std::string &containerId : stickyBottomContainerIds) {
    Clay_ElementId cid = Clay_GetElementId(toClayStr(containerId));
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(cid);
    if (!scrollData.found) continue;
    float maxScrollY = std::max(scrollData.contentDimensions.height - scrollData.scrollContainerDimensions.height, 0.0f);
    float currentY = -scrollData.scrollPosition->y;
    if (currentY >= maxScrollY - kStickyBottomEpsilonPx) {
      applyScrollY(backend, cid.id, maxScrollY, scrollData);
    }
  }
  stickyBottomContainerIds.clear();

  for (auto it = pendingScrollTargets.begin(); it != pendingScrollTargets.end();) {
    PendingScrollTarget &req = *it;
    Clay_ElementId cid = Clay_GetElementId(toClayStr(req.containerId));
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(cid);
    bool resolved = false;
    if (scrollData.found) {
      float maxScrollY = std::max(scrollData.contentDimensions.height - scrollData.scrollContainerDimensions.height, 0.0f);
      float maxScrollX = std::max(scrollData.contentDimensions.width - scrollData.scrollContainerDimensions.width, 0.0f);
      bool ready = true;
      switch (req.kind) {
      case PendingScrollTarget::Kind::Bottom:
        applyScrollY(backend, cid.id, maxScrollY, scrollData);
        break;
      case PendingScrollTarget::Kind::Top:
        applyScrollY(backend, cid.id, 0.0f, scrollData);
        break;
      case PendingScrollTarget::Kind::Left:
        applyScrollX(backend, cid.id, 0.0f, scrollData);
        break;
      case PendingScrollTarget::Kind::Right:
        applyScrollX(backend, cid.id, maxScrollX, scrollData);
        break;
      case PendingScrollTarget::Kind::Offset:
        applyScrollX(backend, cid.id, std::clamp(req.offsetX, 0.0f, maxScrollX), scrollData);
        applyScrollY(backend, cid.id, std::clamp(req.offsetY, 0.0f, maxScrollY), scrollData);
        break;
      case PendingScrollTarget::Kind::Element: {
        Clay_ElementId eid = Clay_GetElementId(toClayStr(req.elementId));
        Clay_ElementData elementData = Clay_GetElementData(eid);
        Clay_ElementData containerData = Clay_GetElementData(cid);
        if (!elementData.found || !containerData.found) {
          ready = false;
          break;
        }
        float contentTopAbsolute = containerData.boundingBox.y + scrollData.scrollPosition->y;
        float contentLeftAbsolute = containerData.boundingBox.x + scrollData.scrollPosition->x;
        float targetY = std::clamp(elementData.boundingBox.y - contentTopAbsolute - req.paddingTop, 0.0f, maxScrollY);
        float targetX = std::clamp(elementData.boundingBox.x - contentLeftAbsolute - req.paddingLeft, 0.0f, maxScrollX);
        applyScrollY(backend, cid.id, targetY, scrollData);
        applyScrollX(backend, cid.id, targetX, scrollData);
        break;
      }
      }
      resolved = ready;
    }
    if (resolved) {
      it = pendingScrollTargets.erase(it);
    } else if (++req.framesWaited > kScrollRequestTimeoutFrames) {
      std::fprintf(stderr, "[n8v] scroll request for container '%s' timed out (container or target element id not found)\n", req.containerId.c_str());
      it = pendingScrollTargets.erase(it);
    } else {
      ++it;
    }
  }
}

void ensureInitialized() {
  if (initialized) return;
  initialized = true;

  uint32_t minMemorySize = Clay_MinMemorySize();
  clayMemory.resize(minMemorySize);
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(minMemorySize, clayMemory.data());

  Clay_Initialize(arena, n8v::activeBackend().windowSize(), Clay_ErrorHandler{clayErrorHandler, nullptr});
  Clay_SetMeasureTextFunction(measureText, nullptr);
  Clay_SetCullingEnabled(!n8v::activeBackend().rendersNativeChrome());
}

float frameDelta() {
  using clock = std::chrono::steady_clock;
  static clock::time_point last;
  static bool started = false;
  clock::time_point now = clock::now();
  if (!started) {
    started = true;
    last = now;
    return 0.0f;
  }
  float delta = std::chrono::duration<float>(now - last).count();
  last = now;
  return std::min(delta, 0.1f);
}

} // namespace

extern "C" {

void n8v_begin_frame(void) {
  ensureInitialized();
  currentDelta = frameDelta();
  widgetOrdinal = 0;
  pendingCursor = n8v::CursorKind::Default;

  n8v::Backend &backend = n8v::activeBackend();
  backend.beginFrame();

  textStorage.clear();
  textStyleStorage.clear();
  widgetMetaStorage.clear();
  resetLeafFrameState();
  resetCheckboxFrameState();
  resetToggleFrameState();
  resetRadioFrameState();
  resetEntryFrameState();
  resetDropdownFrameState();
  resetSliderFrameState();
  resetImageFrameState();
  resetSidebarFrameState();

  Clay_SetLayoutDimensions(backend.windowSize());
  Clay_UpdateScrollContainers(false, backend.consumeScrollDelta(), currentDelta);
  Clay_BeginLayout();
}

void n8v_end_frame(void) {
  Clay_RenderCommandArray commands = Clay_EndLayout(currentDelta);
  n8v::Backend &backend = n8v::activeBackend();
  backend.present(commands);
  backend.setCursor(pendingCursor);
  resolvePendingScrollRequests();
}

namespace {
uint16_t snapToGrid(uint16_t value, float unit) {
  if (unit <= 0.0f || value == 0) return value;
  return (uint16_t)(std::lround((float)value / unit) * unit);
}

uint16_t snapGapToGrid(uint16_t value, float unit) {
  uint16_t rounded = snapToGrid(value, unit);
  if (rounded == 0 && value > 0 && unit > 0.0f) rounded = (uint16_t)unit;
  return rounded;
}
} // namespace

void n8v_open_flex(n8v_flex_options options) {
  openElementMaybeWithId(options.id);
  if (options.clip_vertical && options.stick_to_bottom && options.id && *options.id) {
    stickyBottomContainerIds.emplace_back(options.id);
  }

  n8v::Padding pad = toPadding(options.padding);
  uint16_t gap = options.gap;
  Clay_Dimensions cell = n8v::activeBackend().cellSize();
  if (cell.width > 0.0f && cell.height > 0.0f) {
    bool scrollingThisAxis = options.direction == N8V_DIRECTION_HORIZONTAL ? options.clip_horizontal : options.clip_vertical;
    float gapUnit = options.direction == N8V_DIRECTION_HORIZONTAL ? cell.width : cell.height;
    gap = scrollingThisAxis ? snapToGrid(gap, gapUnit) : snapGapToGrid(gap, gapUnit);
    pad.left = snapToGrid(pad.left, cell.width);
    pad.right = snapToGrid(pad.right, cell.width);
    pad.top = snapToGrid(pad.top, cell.height);
    pad.bottom = snapToGrid(pad.bottom, cell.height);
  }

  Clay_ElementDeclaration decl = {};
  decl.layout.layoutDirection = options.direction == N8V_DIRECTION_HORIZONTAL ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
  decl.layout.childGap = gap;
  decl.layout.padding = n8v::detail::toClay(pad);
  decl.layout.childAlignment = {n8v::detail::toClayX(toAlign(options.h_align)), n8v::detail::toClayY(toAlign(options.v_align))};
  decl.layout.sizing.width = n8v::detail::toClay(toSizing(options.width));
  decl.layout.sizing.height = n8v::detail::toClay(toSizing(options.height));
  decl.clip.horizontal = options.clip_horizontal;
  decl.clip.vertical = options.clip_vertical;
  if ((options.clip_horizontal || options.clip_vertical) && n8v::activeBackend().ownsScrollMath()) {
    decl.clip.childOffset = Clay_GetScrollOffset();
  }

  Clay__ConfigureOpenElement(decl);

  if (options.on_hover && options.id && *options.id) {
    static std::unordered_map<std::string, bool> hoverStates;
    bool hovered = Clay_Hovered();
    bool &stored = hoverStates[options.id];
    if (stored != hovered) {
      stored = hovered;
      options.on_hover(hovered, options.on_hover_userdata);
    }
  }
}

void n8v_close_flex(void) { Clay__CloseElement(); }

bool n8v_is_hovered(const char *id) {
  if (!id || !*id) return false;
  Clay_ElementId elementId = Clay_GetElementId(Clay_String{false, (int32_t)std::strlen(id), id});
  return Clay_PointerOver(elementId);
}

void n8v_scroll_to_bottom(const char *container_id) {
  if (!container_id || !*container_id) return;
  pendingScrollTargets.push_back(PendingScrollTarget{container_id, PendingScrollTarget::Kind::Bottom, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0});
}

void n8v_scroll_to_top(const char *container_id) {
  if (!container_id || !*container_id) return;
  pendingScrollTargets.push_back(PendingScrollTarget{container_id, PendingScrollTarget::Kind::Top, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0});
}

void n8v_scroll_to_left(const char *container_id) {
  if (!container_id || !*container_id) return;
  pendingScrollTargets.push_back(PendingScrollTarget{container_id, PendingScrollTarget::Kind::Left, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0});
}

void n8v_scroll_to_right(const char *container_id) {
  if (!container_id || !*container_id) return;
  pendingScrollTargets.push_back(PendingScrollTarget{container_id, PendingScrollTarget::Kind::Right, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0});
}

void n8v_scroll_to_offset(const char *container_id, float x, float y) {
  if (!container_id || !*container_id) return;
  pendingScrollTargets.push_back(PendingScrollTarget{container_id, PendingScrollTarget::Kind::Offset, {}, x, y, 0.0f, 0.0f, 0});
}

void n8v_scroll_to_element(const char *container_id, const char *element_id, float padding_left, float padding_top) {
  if (!container_id || !*container_id || !element_id || !*element_id) return;
  pendingScrollTargets.push_back(PendingScrollTarget{container_id, PendingScrollTarget::Kind::Element, element_id, 0.0f, 0.0f, padding_left, padding_top, 0});
}

} // extern "C"
