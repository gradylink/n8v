import { loadNative, type NativeAddon, type NativeRef } from "./native.ts";
import {
  ALIGN,
  type Align,
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

export interface PageOptions {
  name?: string;
  icon?: string;
  image?: string;
}

/** Returns true (and only then runs + closes `body`) if this is the selected page - mirrors
 * n8v_open_page/the `page()` macro exactly, including "don't declare children unless shown". */
export function page(opts: PageOptions, body: () => void): boolean {
  const selected = n().openPage(opts);
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
}

export function button(label: string, opts: ButtonOptions = {}): void {
  n().button(
    {
      style: BUTTON_STYLE[opts.style ?? "primary"],
      icon: opts.icon,
      iconVariant: ICON_VARIANT[opts.iconVariant ?? "outline"],
      iconPosition: ICON_POSITION[opts.iconPosition ?? "leading"],
      onClick: opts.onClick,
    },
    label,
  );
}

export interface CheckedOptions {
  checked: Ref<boolean>;
  onChange?: (value: boolean) => void;
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
}

export function entry(opts: EntryOptions): void {
  n().entry(opts);
}

export interface DropdownOptions {
  items: string[];
  selected: Ref<number>;
  placeholder?: string;
  onChange?: (value: number) => void;
}

export function dropdown(opts: DropdownOptions): void {
  n().dropdown(opts);
}

export interface SliderOptions {
  value: Ref<number>;
  min?: number;
  max?: number;
  onChange?: (value: number) => void;
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
