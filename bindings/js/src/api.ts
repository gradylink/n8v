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
  type NativeRounding,
  type NativeSizing,
  type Padding,
  PANEL_ROLE,
  type PanelRole,
  Sizing,
  STYLE_FAMILY,
  type StyleFamily,
  styleFamilyFromNative,
  SUBMIT_MODE,
  type SubmitMode,
} from "./types.ts";

export * from "./types.ts";
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
  });
  body();
  n().closeSidebar();
}

export type BadgeColor =
  | "default"
  | "red"
  | "orange"
  | "yellow"
  | "green"
  | "blue"
  | "purple"
  | "pink";

const BADGE_COLORS = {
  default: 0,
  red: 1,
  orange: 2,
  yellow: 3,
  green: 4,
  blue: 5,
  purple: 6,
  pink: 7,
} as const;

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
}

export function page(opts: PageOptions, body: () => void): boolean {
  const selected = n().openPage({
    ...opts,
    imageRounding: opts.imageRounding,
    badgeColor: BADGE_COLORS[opts.badgeColor ?? "default"],
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
  url?: string;
  onClick?: () => void;
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
}

export function entry(opts: EntryOptions): void {
  n().entry({
    ...opts,
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
  });
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
}

export function document(opts: DocumentOptions): void {
  n().document({
    markdown: opts.markdown,
    width: opts.width ?? Sizing.grow(),
    height: opts.height ?? Sizing.grow(),
    id: opts.id,
  });
}

export interface FloatingOptions {
  width?: NativeSizing;
  height?: NativeSizing;
  elementAttachPoint?: AttachPoint;
  parentAttachPoint?: AttachPoint;
  offsetX?: number;
  offsetY?: number;
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
    id: opts.id,
  });
  body();
  n().closeFloating();
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
