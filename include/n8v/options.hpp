#pragma once

#include <n8v/types.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace n8v {

struct FlexOptions {
  Direction direction = Direction::Horizontal;
  uint16_t gap = 0;
  Padding padding = {};
  Align hAlign = Align::Start;
  Align vAlign = Align::Start;
  Sizing width = Sizing::fit();
  Sizing height = Sizing::fit();
  bool clipHorizontal = false;
  bool clipVertical = false;
  std::string_view id = {};
  bool stickToBottom = false;
  /** Invoked once when the pointer enters or leaves this element, with the new hover state. Requires `.id`. */
  std::function<void(bool)> onHover = {};
};

struct PanelOptions {
  PanelRole role = PanelRole::Card;
  Direction direction = Direction::Vertical;
  uint16_t gap = 0;
  Align hAlign = Align::Start;
  Align vAlign = Align::Start;
  Sizing width = Sizing::fit();
  Sizing height = Sizing::fit();
  bool clipHorizontal = false;
  bool clipVertical = false;
  std::string_view id = {};
};

struct TextOptions {
  bool bold = false;
  bool italic = false;
  bool strikethrough = false;
  /** If non-empty, the text is painted as a link (color + underline) to this URL and opens it on
      click - ignored if onClick is set. */
  std::string_view url = {};
  /** If set, the text is painted as a link (color + underline) and this fires on click instead of
      opening `url`. Takes priority over `url` if both are set. */
  std::function<void()> onClick = nullptr;
  /** Ignored by style families that pick their own color (e.g. links). */
  Color color = {0, 0, 0, 255};
};

struct ButtonOptions {
  ButtonStyle style = ButtonStyle::Secondary;
  /** Fires on press, not release. */
  std::function<void()> onClick = nullptr;
  std::string icon;
  IconVariant iconVariant = IconVariant::Outline;
  IconPosition iconPosition = IconPosition::Leading;
  std::string_view id = {};
};

struct CheckboxOptions {
  bool *checked = nullptr;
  std::function<void(bool)> onChange = nullptr;
  std::string_view id = {};
};

/** A button that stays pressed while `*checked` is true. Draw it with toggleButton(). */
struct ToggleButtonOptions {
  /** Flipped on every click. Required. */
  bool *checked = nullptr;
  std::function<void(bool)> onChange = nullptr;
  ButtonStyle style = ButtonStyle::Secondary;
  std::string icon;
  IconVariant iconVariant = IconVariant::Outline;
  IconPosition iconPosition = IconPosition::Leading;
  std::string_view id = {};
};

struct ToggleOptions {
  bool *checked = nullptr;
  std::function<void(bool)> onChange = nullptr;
  std::string_view id = {};
};

struct RadioOptions {
  /** Shared by every radio button in the same group - selecting one sets *selected to its value. */
  int *selected = nullptr;
  int value = 0;
  std::function<void(int)> onChange = nullptr;
  std::string_view id = {};
};

struct EntryOptions {
  std::string *value = nullptr;
  std::string_view placeholder = {};
  bool password = false;
  std::function<void(std::string_view)> onChange = nullptr;
  std::function<void()> onSubmit = nullptr;
  std::string_view id = {};
  bool multiline = false;
  SubmitMode submitMode = SubmitMode::Enter;
  uint16_t minLines = 3;
  uint16_t maxLines = 0;
  /** GTK4 Only. */
  bool matchEntryStyle = false;
};

struct DropdownOptions {
  std::vector<std::string_view> items = {};
  /** Index into `items`. Out of range (including untouched -1) shows `placeholder`. */
  int *selected = nullptr;
  std::string_view placeholder = {};
  std::function<void(int)> onChange = nullptr;
  std::string_view id = {};
};

struct SliderOptions {
  float *value = nullptr;
  float min = 0.0f;
  float max = 1.0f;
  std::function<void(float)> onChange = nullptr;
  std::string_view id = {};
};

enum class ImageSource {
  Path,
  Bundle,
  Encoded,
  Rgba,
};

struct ImageOptions {
  ImageSource source = ImageSource::Path;
  std::string path;
  const uint8_t *encodedData = nullptr;
  size_t encodedSize = 0;
  const uint8_t *pixels = nullptr;
  int pixelWidth = 0;
  int pixelHeight = 0;
  Sizing width = Sizing::fit();
  Sizing height = Sizing::fit();
  Rounding rounding = Rounding::styleDefault();
};

struct SidebarOptions {
  std::string_view title = {};
  int *selected = nullptr;
  std::function<void(int)> onChange = nullptr;
  Sizing width = Sizing::fixed(240.0f);
  /** 0 = style/backend default. Only affects backends with a resizable sidebar (e.g. GTK4/Adwaita). */
  float minWidth = 0.0f;
  /** 0 = unbounded. Only affects backends with a resizable sidebar (e.g. GTK4/Adwaita). */
  float maxWidth = 0.0f;
  /** Only affects backends with a native sidebar list (e.g. GTK4/Adwaita). */
  bool compact = false;
};

/** Named colors resolved against the active toolkit's palette where it has one. */
enum class BadgeColor {
  Default,
  Red,
  Orange,
  Yellow,
  Green,
  Blue,
  Purple,
  Pink,
};

struct PageOptions {
  std::string_view name = {};
  /** Mutually exclusive with `image`. */
  std::string_view icon = {};
  /** Mutually exclusive with `icon`. */
  std::string_view image = {};
  /** Only affects `.image`; ignored when `.icon` is used. */
  Rounding imageRounding = Rounding::styleDefault();
  /** Draws attention to the row: a marker on its left and an emphasised name. */
  bool notify = false;
  /** Short text shown in a pill on the right of the row (e.g. a count). Empty shows nothing. */
  std::string_view badge = {};
  BadgeColor badgeColor = BadgeColor::Default;
};

struct LinkStyle {
  Color foreground = {0, 0, 0, 0};
  Color background = {0, 0, 0, 0};
  bool bold = false;
};

struct DocumentOptions {
  std::string_view markdown = {};
  Sizing width = Sizing::grow();
  Sizing height = Sizing::grow();
  std::string_view id = {};
  std::function<std::optional<LinkStyle>(std::string_view url)> linkStyle = nullptr;
  std::function<std::string(std::string_view src)> imageResolver = nullptr;
};

/**
 * Layers a child over the currently-open element without affecting its size or the position of its other children
 */
struct FloatingOptions {
  Sizing width = Sizing::fit();
  Sizing height = Sizing::fit();
  AttachPoint elementAttachPoint = AttachPoint::RightTop;
  AttachPoint parentAttachPoint = AttachPoint::RightTop;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  std::string_view id = {};
};

struct IconOptions {
  std::string name;
  IconVariant variant = IconVariant::Outline;
  Sizing width = Sizing::fit();
  Sizing height = Sizing::fit();
  /** Zero-alpha (default) means "use default icon tint". */
  Color tint = {0, 0, 0, 0};
};

struct FileFilter {
  std::string name;
  std::vector<std::string> patterns;
};

struct FileDialogOptions {
  std::string title = {};
  /** Directory the dialog starts in. */
  std::string initialDir = {};
  /** Pre-filled file name (save dialogs only). */
  std::string defaultName = {};
  /** The first filter is selected initially. Empty means all files. */
  std::vector<FileFilter> filters = {};
  /** Allow selecting several files/folders (open dialogs only). */
  bool multiple = false;
};

enum class MessageKind {
  Info,
  Warning,
  Error,
};

struct MessageButton {
  std::string label;
  /** Styled as a destructive action (e.g. red) where the toolkit supports it. */
  bool destructive = false;

  MessageButton(std::string label, bool destructive = false) : label(std::move(label)), destructive(destructive) {}
  MessageButton(const char *label, bool destructive = false) : label(label), destructive(destructive) {}
};

struct MessageDialogOptions {
  std::string title = {};
  std::string message = {};
  MessageKind kind = MessageKind::Info;
  /** The first is the default. Empty shows a single "OK". */
  std::vector<MessageButton> buttons = {};
};

} // namespace n8v
