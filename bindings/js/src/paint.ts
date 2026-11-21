import type { NativeAddon } from "./native.ts";
import {
  BUTTON_STYLE,
  type ButtonStyle,
  type Color,
  type CornerRadius,
  type Padding,
  PANEL_ROLE,
  type PanelRole,
  STYLE_FAMILY,
  type StyleFamily,
} from "./types.ts";

export type Font = "dejavuSans" | "roboto" | "inter" | "selawik";
const FONTS: Font[] = ["dejavuSans", "roboto", "inter", "selawik"];

export interface ButtonPaint {
  background: Color;
  textColor: Color;
  cornerRadius: CornerRadius;
  padding: Padding;
  font: Font;
  fontSize: number;
  transitionSeconds: number;
}

export interface TextPaint {
  color: Color;
  font: Font;
  fontSize: number;
}

export interface CheckboxPaint {
  background: Color;
  borderColor: Color;
  borderWidth: number;
  checkColor: Color;
  cornerRadius: CornerRadius;
  indicatorSize: number;
  padding: Padding;
  font: Font;
  fontSize: number;
  transitionSeconds: number;
  checkMark: number;
}

export interface RadioPaint {
  background: Color;
  borderColor: Color;
  borderWidth: number;
  dotColor: Color;
  indicatorSize: number;
  padding: Padding;
  font: Font;
  fontSize: number;
  transitionSeconds: number;
}

export interface TogglePaint {
  trackOnColor: Color;
  trackOffColor: Color;
  trackBorderColor: Color;
  trackBorderWidth: number;
  knobOnColor: Color;
  knobOffColor: Color;
  knobGlyphColor: Color;
  showGlyphWhenOn: boolean;
  trackWidth: number;
  trackHeight: number;
  knobSizeOff: number;
  knobSizeOn: number;
  padding: Padding;
  font: Font;
  fontSize: number;
  transitionSeconds: number;
}

export interface EntryPaint {
  background: Color;
  textColor: Color;
  placeholderColor: Color;
  borderColor: Color;
  borderWidth: number;
  outlined: boolean;
  labelColor: Color;
  labelFontSize: number;
  transitionSeconds: number;
  cornerRadius: CornerRadius;
  padding: Padding;
  font: Font;
  fontSize: number;
}

export interface DropdownPaint {
  background: Color;
  textColor: Color;
  placeholderColor: Color;
  popupBackground: Color;
  itemHoverBackground: Color;
  itemSelectedBackground: Color;
  cornerRadius: CornerRadius;
  padding: Padding;
  font: Font;
  fontSize: number;
  transitionSeconds: number;
  labelColor: Color;
  labelFontSize: number;
  indicatorColor: Color;
  indicatorWidth: number;
}

export interface SliderPaint {
  trackColor: Color;
  fillColor: Color;
  thumbColor: Color;
  trackHeight: number;
  thumbWidth: number;
  thumbHeight: number;
  thumbBorderColor: Color;
  thumbBorderWidth: number;
  trackGap: number;
}

export interface ImagePaint {
  cornerRadius: CornerRadius;
}

export interface IconPaint {
  tint: Color;
  defaultSize: number;
}

export interface SidebarPaint {
  background: Color;
  borderColor: Color;
  borderWidth: number;
  cornerRadius: CornerRadius;
  padding: Padding;
  rowGap: number;
  rowBackground: Color;
  rowHoverBackground: Color;
  rowSelectedBackground: Color;
  rowTextColor: Color;
  rowSelectedTextColor: Color;
  rowCornerRadius: number;
  rowPadding: Padding;
}

export interface PanelPaint {
  background: Color;
  borderColor: Color;
  borderWidth: number;
  cornerRadius: CornerRadius;
  padding: Padding;
}

/** What `CustomPaint.text` is asked about: the options of the text being drawn. */
export interface TextPaintOptions {
  bold: boolean;
  italic: boolean;
  strikethrough: boolean;
  small: boolean;
  darken: boolean;
  large: boolean;
  clickUnstyled: boolean;
  url?: string;
  color: Color;
}

/**
 * Draws the widgets of the "custom" style. Leave a field out to keep the plain style's look for that widget. Only the SDL2 and HTML
 * backends use it; native toolkits draw their own widgets. Start from `builtinPaint` to change just a few things.
 */
export interface CustomPaint {
  button?(style: ButtonStyle, hovered: boolean, pressed: boolean): ButtonPaint;
  text?(options: TextPaintOptions): TextPaint;
  checkbox?(
    checked: boolean,
    hovered: boolean,
    pressed: boolean,
  ): CheckboxPaint;
  radio?(selected: boolean, hovered: boolean, pressed: boolean): RadioPaint;
  toggle?(on: boolean, hovered: boolean, pressed: boolean): TogglePaint;
  entry?(focused: boolean, hasValue: boolean): EntryPaint;
  dropdown?(
    open: boolean,
    hasSelection: boolean,
    hovered: boolean,
    pressed: boolean,
  ): DropdownPaint;
  slider?(hovered: boolean, pressed: boolean): SliderPaint;
  image?(): ImagePaint;
  icon?(): IconPaint;
  sidebar?(): SidebarPaint;
  panel?(role: PanelRole, hovered: boolean, pressed: boolean): PanelPaint;
  windowBackground?(): Color;
}

// The addon speaks fonts as numbers and enums as their C values; these map to and from the friendlier names.
function fontOut<T extends object>(p: T): T {
  return "font" in p ? { ...p, font: FONTS.indexOf(p.font as Font) } : p;
}
function fontIn<T extends object>(p: T): T {
  return "font" in p
    ? { ...p, font: FONTS[p.font as number] ?? "dejavuSans" }
    : p;
}
const BUTTON_STYLES: ButtonStyle[] = ["secondary", "primary", "ghost"];
const PANEL_ROLES: PanelRole[] = ["card", "listItem"];

/** Installs `paint` and switches to the "custom" style. */
export function setCustomPaint(native: NativeAddon, paint: CustomPaint): void {
  const wrapped: Record<string, (...args: never[]) => unknown> = {};
  if (paint.button) {
    wrapped.button = (style: number, hovered: boolean, pressed: boolean) =>
      fontOut(paint.button!(BUTTON_STYLES[style], hovered, pressed));
  }
  if (paint.text) {
    wrapped.text = (options: TextPaintOptions) => fontOut(paint.text!(options));
  }
  if (paint.checkbox) {
    wrapped.checkbox = (checked: boolean, hovered: boolean, pressed: boolean) =>
      fontOut(paint.checkbox!(checked, hovered, pressed));
  }
  if (paint.radio) {
    wrapped.radio = (selected: boolean, hovered: boolean, pressed: boolean) =>
      fontOut(paint.radio!(selected, hovered, pressed));
  }
  if (paint.toggle) {
    wrapped.toggle = (on: boolean, hovered: boolean, pressed: boolean) =>
      fontOut(paint.toggle!(on, hovered, pressed));
  }
  if (paint.entry) {
    wrapped.entry = (focused: boolean, hasValue: boolean) =>
      fontOut(paint.entry!(focused, hasValue));
  }
  if (paint.dropdown) {
    wrapped.dropdown = (
      open: boolean,
      hasSelection: boolean,
      hovered: boolean,
      pressed: boolean,
    ) => fontOut(paint.dropdown!(open, hasSelection, hovered, pressed));
  }
  if (paint.slider) {
    wrapped.slider = (hovered: boolean, pressed: boolean) =>
      fontOut(paint.slider!(hovered, pressed));
  }
  if (paint.image) wrapped.image = () => fontOut(paint.image!());
  if (paint.icon) wrapped.icon = () => fontOut(paint.icon!());
  if (paint.sidebar) wrapped.sidebar = () => fontOut(paint.sidebar!());
  if (paint.panel) {
    wrapped.panel = (role: number, hovered: boolean, pressed: boolean) =>
      fontOut(paint.panel!(PANEL_ROLES[role], hovered, pressed));
  }
  if (paint.windowBackground) {
    wrapped.windowBackground = () => paint.windowBackground!();
  }
  native.setCustomPaint(wrapped);
}

/**
 * The paint of one of the bundled styles ("plain", "material", "cupertino" or "fluent"). A custom style calls it for the widgets, or
 * the parts of a widget, it does not want to change. `windowBackground` is left out: the bundled styles have no query for it.
 */
export function builtinPaint(
  native: NativeAddon,
  family: StyleFamily,
): Required<Omit<CustomPaint, "windowBackground">> {
  const f = STYLE_FAMILY[family];
  return {
    button: (style: ButtonStyle, hovered: boolean, pressed: boolean) =>
      fontIn(
        native.styleButtonPaint(f, BUTTON_STYLE[style], hovered, pressed),
      ) as ButtonPaint,
    text: (options: TextPaintOptions) =>
      fontIn(native.styleTextPaint(f, options)) as TextPaint,
    checkbox: (checked: boolean, hovered: boolean, pressed: boolean) =>
      fontIn(
        native.styleCheckboxPaint(f, checked, hovered, pressed),
      ) as CheckboxPaint,
    radio: (selected: boolean, hovered: boolean, pressed: boolean) =>
      fontIn(
        native.styleRadioPaint(f, selected, hovered, pressed),
      ) as RadioPaint,
    toggle: (on: boolean, hovered: boolean, pressed: boolean) =>
      fontIn(native.styleTogglePaint(f, on, hovered, pressed)) as TogglePaint,
    entry: (focused: boolean, hasValue: boolean) =>
      fontIn(native.styleEntryPaint(f, focused, hasValue)) as EntryPaint,
    dropdown: (
      open: boolean,
      hasSelection: boolean,
      hovered: boolean,
      pressed: boolean,
    ) =>
      fontIn(
        native.styleDropdownPaint(f, open, hasSelection, hovered, pressed),
      ) as DropdownPaint,
    slider: (hovered: boolean, pressed: boolean) =>
      fontIn(native.styleSliderPaint(f, hovered, pressed)) as SliderPaint,
    image: () => fontIn(native.styleImagePaint(f)) as ImagePaint,
    icon: () => fontIn(native.styleIconPaint(f)) as IconPaint,
    sidebar: () => fontIn(native.styleSidebarPaint(f)) as SidebarPaint,
    panel: (role: PanelRole, hovered: boolean, pressed: boolean) =>
      fontIn(
        native.stylePanelPaint(f, PANEL_ROLE[role], hovered, pressed),
      ) as PanelPaint,
  };
}
