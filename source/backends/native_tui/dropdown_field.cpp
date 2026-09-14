#include "color.hpp"
#include "tui_backend_impl.hpp"

#include "core/native_widget_meta.hpp"

#include <algorithm>

namespace n8v::detail {

namespace {
constexpr int maxPopupRows = 8;
}

void TuiBackend::closeDropdownIfClickedOutside() {
  if (openDropdownOrdinal_ < 0 || !justClicked_) return;
  int pcx = (int)(pointerX_ / cellPxW);
  int pcy = (int)(pointerY_ / cellPxH);
  const CellRect &box = openDropdownBoxRect_;
  bool inBox = pcx >= box.x && pcx < box.x + box.w && pcy >= box.y && pcy < box.y + box.h;
  bool inPopup = pcx >= box.x && pcx < box.x + box.w && pcy >= box.y + box.h && pcy < box.y + box.h + maxPopupRows;
  if (!inBox && !inPopup) {
    openDropdownOrdinal_ = -1;
    openDropdownMeta_ = nullptr;
  }
}

bool TuiBackend::openDropdownPopupOverlapsRows(int rowStart, int rowCount) const {
  if (openDropdownOrdinal_ < 0 || !openDropdownMeta_ || !openDropdownMeta_->dropdownItems) return false;
  int popupCount = std::min((int)openDropdownMeta_->dropdownItems->size(), maxPopupRows);
  if (popupCount <= 0) return false;
  int popupStart = openDropdownBoxRect_.y + openDropdownBoxRect_.h;
  int popupEnd = popupStart + popupCount;
  return rowStart < popupEnd && rowStart + rowCount > popupStart;
}

void TuiBackend::renderDropdownBox(NativeWidgetMeta &meta, const Clay_RenderCommand &command) {
  CellRect rect = cellRect(command.boundingBox);
  const Clay_Color &color = command.renderData.rectangle.backgroundColor;
  if (color.a > 0.0f) drawRoundedRect(command.boundingBox, color, command.renderData.rectangle.cornerRadius);
  if (rect.w <= 0 || rect.h <= 0) return;

  bool hitBox = Clay_PointerOver(Clay_ElementId{command.id});
  bool isOpen = openDropdownOrdinal_ == meta.ordinal;

  if (justClicked_ && hitBox) {
    if (isOpen) {
      openDropdownOrdinal_ = -1;
      isOpen = false;
    } else {
      openDropdownOrdinal_ = meta.ordinal;
      openDropdownBoxRect_ = rect;
      isOpen = true;
      dropdownHighlightIndex_ = meta.dropdownSelected ? *meta.dropdownSelected : 0;
    }
  } else if (justClicked_ && isOpen && meta.dropdownItems) {
    int pcx = (int)(pointerX_ / cellPxW);
    int pcy = (int)(pointerY_ / cellPxH);
    int row = pcy - (rect.y + rect.h);
    bool inCols = pcx >= rect.x && pcx < rect.x + rect.w;
    if (row >= 0 && row < (int)meta.dropdownItems->size() && row < maxPopupRows && inCols) {
      if (meta.dropdownSelected) *meta.dropdownSelected = row;
      if (meta.onDropdownChange) meta.onDropdownChange(row, meta.onDropdownChangeUserdata);
      openDropdownOrdinal_ = -1;
      isOpen = false;
    }
  }

  Clay_Color chevronColor{120, 120, 120, 255};
  Clay_BoundingBox chevronBox{command.boundingBox.x + command.boundingBox.width - cellPxW, command.boundingBox.y, cellPxW, command.boundingBox.height};
  drawDropdownChevron(chevronBox, chevronColor, isOpen);

  if (isOpen) {
    openDropdownBoxRect_ = rect;
    openDropdownMeta_ = &meta;
  } else if (openDropdownMeta_ == &meta) {
    openDropdownMeta_ = nullptr;
  }
}

void TuiBackend::renderDropdownPopup(NativeWidgetMeta &meta, const CellRect &boxRect) {
  if (!meta.dropdownItems) return;
  int count = std::min((int)meta.dropdownItems->size(), maxPopupRows);
  for (int i = 0; i < count; ++i) {
    CellRect rowRect{boxRect.x, boxRect.y + boxRect.h + i, boxRect.w, 1};
    bool selected = meta.dropdownSelected && *meta.dropdownSelected == i;
    bool highlighted = i == dropdownHighlightIndex_;
    ftxui::Color bg = highlighted ? ftxui::Color::RGB(190, 210, 245) : selected ? ftxui::Color::RGB(220, 230, 250) : ftxui::Color::RGB(255, 255, 255);
    grid_.fillBackground(rowRect, bg);
    drawTextAt(rowRect.x, rowRect.y, (*meta.dropdownItems)[i], Clay_Color{20, 20, 20, 255});
  }
}

void TuiBackend::moveDropdownHighlight(int delta) {
  if (openDropdownOrdinal_ < 0 || !openDropdownMeta_ || !openDropdownMeta_->dropdownItems) return;
  int count = std::min((int)openDropdownMeta_->dropdownItems->size(), maxPopupRows);
  if (count <= 0) return;
  if (dropdownHighlightIndex_ < 0) dropdownHighlightIndex_ = 0;
  dropdownHighlightIndex_ = std::clamp(dropdownHighlightIndex_ + delta, 0, count - 1);
}

void TuiBackend::confirmDropdownHighlighted() {
  if (openDropdownOrdinal_ < 0 || !openDropdownMeta_ || !openDropdownMeta_->dropdownItems) return;
  int count = (int)openDropdownMeta_->dropdownItems->size();
  if (dropdownHighlightIndex_ < 0 || dropdownHighlightIndex_ >= count) return;
  NativeWidgetMeta *meta = openDropdownMeta_;
  int index = dropdownHighlightIndex_;
  if (meta->dropdownSelected) *meta->dropdownSelected = index;
  if (meta->onDropdownChange) meta->onDropdownChange(index, meta->onDropdownChangeUserdata);
  openDropdownOrdinal_ = -1;
  openDropdownMeta_ = nullptr;
}

} // namespace n8v::detail
