import { loadNative, type NativeAddon, type NativeRef } from "./native.ts";
import {
  ALIGN,
  type Align,
  ATTACH_POINT,
  type AttachPoint,
  BUTTON_STYLE,
  type ButtonStyle,
  type Color,
  DIRECTION,
  type Direction,
  ICON_POSITION,
  ICON_VARIANT,
  type IconPosition,
  type IconVariant,
  NAMED_COLOR,
  type NativeRounding,
  type NativeSizing,
  type NamedColor,
  type Padding,
  PANEL_ROLE,
  type PanelRole,
  POPOVER_SIDE,
  type PopoverSide,
  Sizing,
  STYLE_FAMILY,
  type StyleFamily,
  styleFamilyFromNative,
  SUBMIT_MODE,
  type SubmitMode,
} from "./types.ts";

import {
  builtinPaint as builtinPaintImpl,
  type CustomPaint,
  setCustomPaint as setCustomPaintImpl,
} from "./paint.ts";

export * from "./types.ts";
export type * from "./paint.ts";
export type { NativeRef };

let native: NativeAddon | null = null;
function n(): NativeAddon {
  return native ??= loadNative();
}

/** A mutable cell n8v reads/writes across frames - this is similar to `ref`s from JS frameworks like Vue. Construct with `Ref.bool()`, `Ref.int()`, etc. */
export type Ref<T> = NativeRef<T>;

export interface RefFactories {
  bool(initial?: boolean): Ref<boolean>;
  int(initial?: number): Ref<number>;
  float(initial?: number): Ref<number>;
  string(initial?: string): Ref<string>;
}

export const Ref: RefFactories = {
  bool(initial = false): Ref<boolean> {
    return new (n().BoolRef)(initial);
  },
  int(initial = 0): Ref<number> {
    return new (n().IntRef)(initial);
  },
  float(initial = 0): Ref<number> {
    return new (n().FloatRef)(initial);
  },
  string(initial = ""): Ref<string> {
    return new (n().StringRef)(initial);
  },
};

export function ref(initial: string): Ref<string>;
export function ref(initial: boolean): Ref<boolean>;
export function ref(initial: number): Ref<number>;
export function ref(
  initial: string | boolean | number,
): Ref<string> | Ref<boolean> | Ref<number> {
  if (typeof initial === "string") return Ref.string(initial);
  if (typeof initial === "boolean") return Ref.bool(initial);
  return Ref.float(initial);
}

/** Opens the native window. Call once before the render loop. */
export function initialize(
  width: number,
  height: number,
  title: string,
): boolean {
  return n().initialize(width, height, title);
}

/** Pumps OS/backend events; returns false once the window has been closed. */
export function pumpEvents(): boolean {
  return n().pumpEvents();
}

export function shutdown(): void {
  n().shutdown();
}

/** Installs `paint` and switches to the "custom" style. Only the SDL2 and HTML backends use it. */
export function setCustomPaint(paint: CustomPaint): void {
  setCustomPaintImpl(n(), paint);
}

/** The paint of one of the bundled styles, for a custom style to delegate to for whatever it does not change. */
export function builtinPaint(family: StyleFamily) {
  return builtinPaintImpl(n(), family);
}

export function setStyleFamily(family: StyleFamily): void {
  n().setStyleFamily(STYLE_FAMILY[family]);
}

export function activeStyleFamily(): StyleFamily {
  return styleFamilyFromNative(n().activeStyleFamily());
}

/** Declares one frame: `body` runs between n8v_begin_frame/n8v_end_frame. */
export function frame(body: () => void): void {
  n().beginFrame();
  body();
  n().endFrame();
}

/**
 * `initialize` + a `while (pumpEvents())` loop calling `body` each frame, then `shutdown`. The
 * common case - reach for `initialize`/`pumpEvents`/`frame`/`shutdown` directly if you need to
 * interleave other work (a network poll, a timer) with the render loop.
 */
export function run(
  width: number,
  height: number,
  title: string,
  body: () => void,
): void {
  if (!initialize(width, height, title)) {
    throw new Error("n8v: initialize() failed");
  }
  try {
    while (pumpEvents()) {
      frame(body);
    }
  } finally {
    shutdown();
  }
}

export interface FlexOptions {
  direction?: Direction;
  gap?: number;
  padding?: Padding;
  hAlign?: Align;
  vAlign?: Align;
  width?: NativeSizing;
  height?: NativeSizing;
  clipHorizontal?: boolean;
  clipVertical?: boolean;
  id?: string;
  stickToBottom?: boolean;
  onHover?: (hovered: boolean) => void;
}

function flexNative(o: FlexOptions = {}) {
  return {
    direction: DIRECTION[o.direction ?? "horizontal"],
    gap: o.gap ?? 0,
    padding: o.padding ?? { left: 0, right: 0, top: 0, bottom: 0 },
    hAlign: ALIGN[o.hAlign ?? "start"],
    vAlign: ALIGN[o.vAlign ?? "start"],
    width: o.width ?? Sizing.fit(),
    height: o.height ?? Sizing.fit(),
    clipHorizontal: o.clipHorizontal ?? false,
    clipVertical: o.clipVertical ?? false,
    id: o.id,
    stickToBottom: o.stickToBottom ?? false,
    onHover: o.onHover,
  };
}

export function flex(opts: FlexOptions, body: () => void): void {
  n().openFlex(flexNative(opts));
  body();
  n().closeFlex();
}

export interface PanelOptions {
  role?: PanelRole;
  direction?: Direction;
  gap?: number;
  hAlign?: Align;
  vAlign?: Align;
  width?: NativeSizing;
  height?: NativeSizing;
  clipHorizontal?: boolean;
  clipVertical?: boolean;
  id?: string;
  /** A colored bar down the panel's left edge. Alpha 0 means none. */
  accent?: Color;
  /** Overrides the style's padding for this panel. */
  padding?: Padding;
}

export function panel(opts: PanelOptions, body: () => void): void {
  const o = opts;
  n().openPanel({
    role: PANEL_ROLE[o.role ?? "card"],
    direction: DIRECTION[o.direction ?? "vertical"],
    gap: o.gap ?? 0,
    hAlign: ALIGN[o.hAlign ?? "start"],
    vAlign: ALIGN[o.vAlign ?? "start"],
    width: o.width ?? Sizing.fit(),
    height: o.height ?? Sizing.fit(),
    clipHorizontal: o.clipHorizontal ?? false,
    clipVertical: o.clipVertical ?? false,
    id: o.id,
    accent: o.accent,
    padding: o.padding,
  });
  body();
  n().closePanel();
}

export interface SidebarOptions {
  title?: string;
  selected: Ref<number>;
  onChange?: (value: number) => void;
  width?: NativeSizing;
  minWidth?: number;
  maxWidth?: number;
  compact?: boolean;
  /** Makes the sidebar collapsible to a strip of icons: read for the initial state and written on every toggle, so you can persist it. */
  collapsed?: Ref<boolean>;
  onCollapse?: (collapsed: boolean) => void;
}

export function sidebar(opts: SidebarOptions, body: () => void): void {
  n().openSidebar({
    title: opts.title ?? "",
    selected: opts.selected,
    onChange: opts.onChange,
    width: opts.width ?? Sizing.fixed(240),
    minWidth: opts.minWidth ?? 0,
    maxWidth: opts.maxWidth ?? 0,
    compact: opts.compact ?? false,
    collapsed: opts.collapsed,
    onCollapse: opts.onCollapse,
  });
  body();
  n().closeSidebar();
}

/** A titled heading between a sidebar's pages. Only valid inside `sidebar`. */
export function sidebarSection(title: string): void {
  n().sidebarSection(title);
}

/** A rule between a sidebar's pages. Only valid inside `sidebar`. */
export function sidebarDivider(): void {
  n().sidebarDivider();
}

export type BadgeColor = NamedColor;

export interface PageOptions {
  name?: string;
  icon?: string;
  image?: string;
  imageRounding?: NativeRounding;
  /** Draws attention to the row: a marker on its left and an emphasised name. */
  notify?: boolean;
  /** Short text shown in a pill on the right of the row, e.g. a count. */
  badge?: string;
  /** Resolved against the toolkit's palette where it has one. */
  badgeColor?: BadgeColor;
  /** Size of `image` in pixels; 0 or omitted for the usual size. */
  imageSize?: number;
}

export function page(opts: PageOptions, body: () => void): boolean {
  const selected = n().openPage({
    ...opts,
    imageRounding: opts.imageRounding,
    badgeColor: NAMED_COLOR[opts.badgeColor ?? "default"],
  });
  if (selected) {
    body();
    n().closePage();
  }
  return selected;
}

export interface TextOptions {
  bold?: boolean;
  italic?: boolean;
  strikethrough?: boolean;
  /** Secondary text: smaller than normal, by whatever the style considers small. */
  small?: boolean;
  /** Secondary text: fainter than normal. */
  darken?: boolean;
  /** Jumbo text, for things like a message that is only emoji. */
  large?: boolean;
  url?: string;
  /** Paints the text as a link and fires on click instead of opening `url`. */
  onClick?: () => void;
  /** With `onClick`, keeps the text's own look instead of painting it as a link. */
  clickUnstyled?: boolean;
  color?: Color;
}

export function text(label: string, opts: TextOptions = {}): void {
  n().text(opts, label);
}

export interface ButtonOptions {
  style?: ButtonStyle;
  icon?: string;
  iconVariant?: IconVariant;
  iconPosition?: IconPosition;
  onClick?: () => void;
  id?: string;
  /** An image path shown in place of an icon. */
  image?: string;
  /** Height of `image` in pixels; 0 or omitted for the usual size. */
  imageHeight?: number;
}

export function button(label: string, opts: ButtonOptions = {}): void {
  n().button(
    {
      style: BUTTON_STYLE[opts.style ?? "secondary"],
      icon: opts.icon,
      iconVariant: ICON_VARIANT[opts.iconVariant ?? "outline"],
      iconPosition: ICON_POSITION[opts.iconPosition ?? "leading"],
      onClick: opts.onClick,
      id: opts.id,
      image: opts.image,
      imageHeight: opts.imageHeight ?? 0,
    },
    label,
  );
}

export interface ToggleButtonOptions extends Omit<ButtonOptions, "onClick"> {
  /** Flipped on every click; the button is drawn pressed while it is true. */
  checked: Ref<boolean>;
  onChange?: (value: boolean) => void;
}

/** A button that stays pressed while `checked` is true, with the same icon and style options as `button`. */
export function toggleButton(label: string, opts: ToggleButtonOptions): void {
  n().button(
    {
      style: BUTTON_STYLE[opts.style ?? "secondary"],
      icon: opts.icon,
      iconVariant: ICON_VARIANT[opts.iconVariant ?? "outline"],
      iconPosition: ICON_POSITION[opts.iconPosition ?? "leading"],
      checked: opts.checked,
      onChange: opts.onChange,
      id: opts.id,
      image: opts.image,
      imageHeight: opts.imageHeight ?? 0,
    },
    label,
  );
}

export interface CheckedOptions {
  checked: Ref<boolean>;
  onChange?: (value: boolean) => void;
  id?: string;
}

export function checkbox(label: string, opts: CheckedOptions): void {
  n().checkbox(opts, label);
}

export function toggle(label: string, opts: CheckedOptions): void {
  n().toggle(opts, label);
}

export interface RadioOptions {
  selected: Ref<number>;
  value: number;
  onChange?: (value: number) => void;
  id?: string;
}

export function radio(label: string, opts: RadioOptions): void {
  n().radio(opts, label);
}

export interface EntryOptions {
  value: Ref<string>;
  placeholder?: string;
  password?: boolean;
  onChange?: (text: string) => void;
  onSubmit?: () => void;
  id?: string;
  multiline?: boolean;
  submitMode?: SubmitMode;
  minLines?: number;
  maxLines?: number;
  matchEntryStyle?: boolean;
  /** Return true to stop the entry acting on the key. Multiline entries only, so far, and not in every backend. */
  onKey?: (key: Key, mods: KeyMods) => boolean;
  /** Called with the paths when files are pasted. Multiline entries only, so far, and not in every backend. */
  onPasteFiles?: (paths: string[]) => void;
}

export type Key = "up" | "down" | "escape";

export interface KeyMods {
  shift: boolean;
  ctrl: boolean;
  alt: boolean;
}

const KEYS: Key[] = ["up", "down", "escape"];

export function entry(opts: EntryOptions): void {
  const { onKey } = opts;
  n().entry({
    ...opts,
    onKey: onKey && ((key: number, mods: number) =>
      onKey(KEYS[key], {
        shift: (mods & 1) !== 0,
        ctrl: (mods & 2) !== 0,
        alt: (mods & 4) !== 0,
      })),
    submitMode: SUBMIT_MODE[opts.submitMode ?? "enter"],
    minLines: opts.minLines ?? 0,
    maxLines: opts.maxLines ?? 0,
  });
}

export interface DropdownOptions {
  items: string[];
  selected: Ref<number>;
  placeholder?: string;
  onChange?: (value: number) => void;
  id?: string;
}

export function dropdown(opts: DropdownOptions): void {
  n().dropdown(opts);
}

export interface SliderOptions {
  value: Ref<number>;
  min?: number;
  max?: number;
  onChange?: (value: number) => void;
  id?: string;
}

export function slider(opts: SliderOptions): void {
  n().slider({ ...opts, min: opts.min ?? 0, max: opts.max ?? 1 });
}

export interface ImageOptions {
  /** A file path, or omit and pass `encodedData` for in-memory PNG/JPEG/SVG bytes. */
  path?: string;
  encodedData?: Uint8Array;
  pixels?: Uint8Array;
  pixelWidth?: number;
  pixelHeight?: number;
  width?: NativeSizing;
  height?: NativeSizing;
  rounding?: NativeRounding;
  /** Makes the image clickable. */
  onClick?: () => void;
  /** Like `onClick`, but told where in the image (each axis from 0 to 1). Wins over `onClick` where the toolkit can say where. */
  onClickAt?: (x: number, y: number) => void;
  /** Pinch and Ctrl+wheel zoom: `factor` is the change since the last call (above 1 zooms in) around (x, y), each from 0 to 1. */
  onZoom?: (factor: number, x: number, y: number) => void;
  /** Dragging, with the change in position in pixels since the last call, for panning. */
  onDrag?: (dx: number, dy: number) => void;
}

export function image(opts: ImageOptions): void {
  const sourceKind = opts.pixels
    ? 3 /* RGBA */
    : opts.encodedData
    ? 2 /* ENCODED */
    : 0 /* PATH */;
  n().image({
    sourceKind,
    path: opts.path,
    encodedData: opts.encodedData,
    pixels: opts.pixels,
    pixelWidth: opts.pixelWidth ?? 0,
    pixelHeight: opts.pixelHeight ?? 0,
    width: opts.width ?? Sizing.fit(),
    height: opts.height ?? Sizing.fit(),
    rounding: opts.rounding,
    onClick: opts.onClick,
    onClickAt: opts.onClickAt,
    onZoom: opts.onZoom,
    onDrag: opts.onDrag,
  });
}

export interface VideoOptions {
  /** A file path, or a URL (anything with a scheme, like https://). */
  source: string;
  width?: NativeSizing;
  height?: NativeSizing;
  rounding?: NativeRounding;
  autoplay?: boolean;
  loop?: boolean;
  muted?: boolean;
  /** Show play/pause and a seek bar. */
  controls?: boolean;
  id?: string;
}

export function video(opts: VideoOptions): void {
  n().video({
    ...opts,
    width: opts.width ?? Sizing.fit(),
    height: opts.height ?? Sizing.fit(),
  });
}

export interface AudioOptions {
  /** A file path, or a URL (anything with a scheme, like https://). */
  source: string;
  /** Fit means 320 wide. */
  width?: NativeSizing;
  autoplay?: boolean;
  loop?: boolean;
  muted?: boolean;
  id?: string;
}

/** A player bar: play/pause, a seek bar and the time. */
export function audio(opts: AudioOptions): void {
  n().audio({ ...opts, width: opts.width ?? Sizing.fit() });
}

export interface DividerOptions {
  /** Alpha 0 (the default) means the style's own divider color. */
  color?: Color;
  id?: string;
}

export function divider(opts: DividerOptions = {}): void {
  n().divider(opts);
}

export interface IconOptions {
  name: string;
  variant?: IconVariant;
  width?: NativeSizing;
  height?: NativeSizing;
  tint?: Color;
}

export function icon(opts: IconOptions): void {
  n().icon({
    name: opts.name,
    variant: ICON_VARIANT[opts.variant ?? "outline"],
    width: opts.width ?? Sizing.fit(),
    height: opts.height ?? Sizing.fit(),
    tint: opts.tint,
  });
}

export interface DocumentOptions {
  markdown: string;
  width?: NativeSizing;
  height?: NativeSizing;
  id?: string;
  /** Restyle chosen links; return nothing to leave a link alone. */
  linkStyle?: (url: string) => LinkStyle | null | undefined;
  /** Maps each image `src` to something n8v can load (a file path, say); return "" for none. */
  imageResolver?: (src: string) => string;
  /** Names where the document keeps what it has loaded for its images. */
  imageState?: string;
}

export interface LinkStyle {
  foreground?: Color;
  background?: Color;
  bold?: boolean;
  /** Keeps the link clickable (a styled link is otherwise plain text). */
  clickable?: boolean;
}

export function document(opts: DocumentOptions): void {
  n().document({
    markdown: opts.markdown,
    width: opts.width ?? Sizing.grow(),
    height: opts.height ?? Sizing.grow(),
    id: opts.id,
    linkStyle: opts.linkStyle,
    imageResolver: opts.imageResolver,
    imageState: opts.imageState,
  });
}

export interface FloatingOptions {
  width?: NativeSizing;
  height?: NativeSizing;
  elementAttachPoint?: AttachPoint;
  parentAttachPoint?: AttachPoint;
  offsetX?: number;
  offsetY?: number;
  /** Swallows the pointer instead of letting it through to what is underneath. */
  opaque?: boolean;
  /** Fires on a click outside the element. */
  onDismiss?: () => void;
  /** Nudges the element back inside the window, `screenMargin` pixels from its edges. */
  keepOnScreen?: boolean;
  screenMargin?: number;
  id?: string;
}

export function floating(opts: FloatingOptions, body: () => void): void {
  n().openFloating({
    width: opts.width ?? Sizing.fit(),
    height: opts.height ?? Sizing.fit(),
    elementAttachPoint: ATTACH_POINT[opts.elementAttachPoint ?? "rightTop"],
    parentAttachPoint: ATTACH_POINT[opts.parentAttachPoint ?? "rightTop"],
    offsetX: opts.offsetX ?? 0,
    offsetY: opts.offsetY ?? 0,
    opaque: opts.opaque ?? false,
    onDismiss: opts.onDismiss,
    keepOnScreen: opts.keepOnScreen ?? false,
    screenMargin: opts.screenMargin ?? 0,
    id: opts.id,
  });
  body();
  n().closeFloating();
}

export interface PopoverOptions {
  /** Which side of its parent it opens on. Defaults to "below". */
  side?: PopoverSide;
  /** Lines the far edge, not the near one, up with the parent's. */
  alignEnd?: boolean;
  width?: NativeSizing;
  height?: NativeSizing;
  /** Space in pixels between the popover and its parent. */
  gap?: number;
  screenMargin?: number;
  /** Fires on a click outside. */
  onDismiss?: () => void;
  id?: string;
}

/** Opens `body` beside the enclosing element, layered on top of everything else. */
export function popover(opts: PopoverOptions, body: () => void): void {
  n().openPopover({
    side: POPOVER_SIDE[opts.side ?? "below"],
    alignEnd: opts.alignEnd ?? false,
    width: opts.width ?? Sizing.fit(),
    height: opts.height ?? Sizing.fit(),
    gap: opts.gap ?? 0,
    screenMargin: opts.screenMargin ?? 0,
    onDismiss: opts.onDismiss,
    id: opts.id,
  });
  body();
  n().closePopover();
}

/** Gives keyboard focus to the widget with this id. */
export function focus(id: string): void {
  n().focus(id);
}

export interface ScrollState {
  offsetX: number;
  offsetY: number;
  viewWidth: number;
  viewHeight: number;
}

/** Where a scrolling container is scrolled to and how big its view is, in pixels; null if there is no such container or the backend cannot say. */
export function scrollState(containerId: string): ScrollState | null {
  return n().scrollState(containerId);
}

export interface VisibleChild {
  id: string;
  top: number;
  height: number;
}

/** The id'd children of a scrolling container in view or within `margin` pixels of it; null if there is no such container or the backend cannot say. */
export function visibleChildren(
  containerId: string,
  margin = 0,
): VisibleChild[] | null {
  return n().visibleChildren(containerId, margin);
}

/** Opens a link in the default application (the browser for a web address). */
export function openUrl(url: string): void {
  n().openUrl(url);
}

/** Sees every link before it is opened: return true to say it was handled, false to let n8v open it. Pass null to remove it. */
export function setUrlHandler(
  handler: ((url: string) => boolean) | null,
): void {
  n().setUrlHandler(handler);
}

/** Makes `pumpEvents` return promptly, e.g. when a timer or network callback has work for the UI. */
export function wake(): void {
  n().wake();
}

/** Brings the window to the front. */
export function raiseWindow(): void {
  n().raiseWindow();
}

/** Whether the UI currently has a dark appearance. */
export function isDarkTheme(): boolean {
  return n().isDarkTheme();
}

/** The window's size in logical pixels; null if the backend cannot say. */
export function windowSize(): { width: number; height: number } | null {
  return n().windowSize();
}

export function isHovered(id: string): boolean {
  return n().isHovered(id);
}

/** Scrolls the container to the bottom of its content (vertical axis only). */
export function scrollToBottom(containerId: string): void {
  n().scrollToBottom(containerId);
}

export function scrollToTop(containerId: string): void {
  n().scrollToTop(containerId);
}

export function scrollToLeft(containerId: string): void {
  n().scrollToLeft(containerId);
}

export function scrollToRight(containerId: string): void {
  n().scrollToRight(containerId);
}

/** Scrolls to an absolute pixel offset from the content's top-left, each axis clamped independently. */
export function setScrollOffset(
  containerId: string,
  x: number,
  y: number,
): void {
  n().scrollToOffset(containerId, x, y);
}

/** Scrolls so the element tagged `elementId` is at the container's top-left, offset by the padding. */
export function scrollToElement(
  containerId: string,
  elementId: string,
  paddingLeft = 0,
  paddingTop = 0,
): void {
  n().scrollToElement(containerId, elementId, paddingLeft, paddingTop);
}

export interface FileFilter {
  name: string;
  patterns: string[];
}

export interface FileDialogOptions {
  title?: string;
  /** Directory the dialog starts in. */
  initialDir?: string;
  /** Pre-filled file name (`saveFile` only). */
  defaultName?: string;
  /** Shown in a type dropdown; the first is selected. Omit for all files. */
  filters?: FileFilter[];
  /** Allow selecting several files/folders (not `saveFile`). */
  multiple?: boolean;
}

export type FileDialogCallback = (paths: string[]) => void;

const FILE_DIALOG_KIND = { openFile: 0, saveFile: 1, openFolder: 2 } as const;

function showFileDialog(
  kind: number,
  opts: FileDialogOptions,
  onResult: FileDialogCallback,
): void {
  n().showFileDialog({ ...opts, kind }, onResult);
}

/** Shows the system open dialog without blocking; `onResult` runs from a later `pumpEvents()`. */
export function openFile(
  opts: FileDialogOptions,
  onResult: FileDialogCallback,
): void {
  showFileDialog(FILE_DIALOG_KIND.openFile, opts, onResult);
}

/** Shows the system save dialog; the result is the single path to write to. */
export function saveFile(
  opts: FileDialogOptions,
  onResult: FileDialogCallback,
): void {
  showFileDialog(FILE_DIALOG_KIND.saveFile, opts, onResult);
}

/** Shows the system folder chooser. */
export function openFolder(
  opts: FileDialogOptions,
  onResult: FileDialogCallback,
): void {
  showFileDialog(FILE_DIALOG_KIND.openFolder, opts, onResult);
}

export interface MessageButton {
  label: string;
  /** Styled as a dangerous action (e.g. red) where the toolkit supports it. */
  destructive?: boolean;
}

export interface MessageDialogOptions {
  title?: string;
  message?: string;
  kind?: "info" | "warning" | "error";
  /** The first is the default. A plain string is a normal button. Omit for a single "OK". */
  buttons?: (string | MessageButton)[];
}

/** The index of the pressed button, or -1 if the dialog was dismissed. */
export type MessageDialogCallback = (button: number) => void;

const MESSAGE_KIND = { info: 0, warning: 1, error: 2 } as const;

/** Shows a message dialog without blocking; `onResult` runs from a later `pumpEvents()`. */
export function showMessage(
  opts: MessageDialogOptions,
  onResult?: MessageDialogCallback,
): void {
  n().showMessageDialog(
    { ...opts, kind: MESSAGE_KIND[opts.kind ?? "info"] },
    onResult,
  );
}
