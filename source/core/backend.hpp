#pragma once

#include <n8v/types.hpp>

#include <clay.h>

#include <string_view>

namespace n8v {

struct Backend {
  virtual ~Backend() = default;

  /**
   * @param width Initial window width in pixels.
   * @param height Initial window height in pixels.
   * @return Whether initialization succeeded.
   */
  virtual bool initialize(int width, int height, std::string_view title) = 0;

  /** @return Whether the window should stay open. */
  virtual bool pumpEvents() = 0;

  /** @return Whether the primary pointer button is held this frame. */
  virtual bool pointerDown() const = 0;

  virtual Clay_Dimensions windowSize() const = 0;

  /**
   * @param bold Whether to measure the bold variant of the font.
   * @param italic Whether to measure the italic variant of the font.
   */
  virtual Clay_Dimensions measureText(std::string_view text, FontFamily family, uint16_t fontSize, bool bold, bool italic) const = 0;

  virtual Clay_Dimensions measureNativeChrome(NativeWidgetKind, std::string_view, uint16_t, bool hasIcon = false) const {
    (void)hasIcon;
    return {0, 0};
  }

  // for TUI backend
  virtual Clay_Dimensions cellSize() const { return {0, 0}; }

  virtual bool isEntryFocused(int) const { return false; }

  virtual bool rendersNativeChrome() const { return true; }

  virtual bool aliasesSwitchAsCheckbox() const { return false; }

  virtual Clay_Vector2 consumeScrollDelta() { return {0, 0}; }
  virtual bool ownsScrollMath() const { return false; }

  virtual void beginFrame() = 0;
  virtual void present(Clay_RenderCommandArray commands) = 0;
  virtual void setCursor(CursorKind cursor) = 0;
  virtual void shutdown() = 0;
};

Backend &activeBackend();

} // namespace n8v
