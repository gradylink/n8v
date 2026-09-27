export type Direction = "horizontal" | "vertical";
export type Align = "start" | "center" | "end";
export type ButtonStyle = "primary" | "secondary" | "ghost";
export type PanelRole = "card" | "listItem";
export type IconVariant = "outline" | "filled";
export type IconPosition = "leading" | "trailing";
export type StyleFamily =
  | "plain"
  | "material"
  | "cupertino"
  | "fluent"
  | "custom";

export const DIRECTION: Record<Direction, number> = {
  horizontal: 0,
  vertical: 1,
};
export const ALIGN: Record<Align, number> = { start: 0, center: 1, end: 2 };
export const BUTTON_STYLE: Record<ButtonStyle, number> = {
  primary: 0,
  secondary: 1,
  ghost: 2,
};
export const PANEL_ROLE: Record<PanelRole, number> = { card: 0, listItem: 1 };
export const ICON_VARIANT: Record<IconVariant, number> = {
  outline: 0,
  filled: 1,
};
export const ICON_POSITION: Record<IconPosition, number> = {
  leading: 0,
  trailing: 1,
};
export const STYLE_FAMILY: Record<StyleFamily, number> = {
  plain: 0,
  material: 1,
  cupertino: 2,
  fluent: 3,
  custom: 4,
};
const STYLE_FAMILY_REVERSE: StyleFamily[] = [
  "plain",
  "material",
  "cupertino",
  "fluent",
  "custom",
];
export function styleFamilyFromNative(n: number): StyleFamily {
  return STYLE_FAMILY_REVERSE[n] ?? "plain";
}

const SIZING_MODE = { fit: 0, grow: 1, fixed: 2, percent: 3 } as const;

export interface NativeSizing {
  mode: number;
  value: number;
  min: number;
  max: number;
}

export interface SizingConstructors {
  fit(min?: number, max?: number): NativeSizing;
  grow(min?: number, max?: number): NativeSizing;
  fixed(pixels: number): NativeSizing;
  percent(fraction: number): NativeSizing;
}

export const Sizing: SizingConstructors = {
  fit(min = 0, max = 0): NativeSizing {
    return { mode: SIZING_MODE.fit, value: 0, min, max };
  },
  grow(min = 0, max = 0): NativeSizing {
    return { mode: SIZING_MODE.grow, value: 0, min, max };
  },
  fixed(pixels: number): NativeSizing {
    return { mode: SIZING_MODE.fixed, value: pixels, min: 0, max: 0 };
  },
  percent(fraction: number): NativeSizing {
    return { mode: SIZING_MODE.percent, value: fraction, min: 0, max: 0 };
  },
};

export interface Color {
  r: number;
  g: number;
  b: number;
  a: number;
}

export function rgba(r: number, g: number, b: number, a = 1): Color {
  return { r, g, b, a };
}

export interface Padding {
  left: number;
  right: number;
  top: number;
  bottom: number;
}

export function padding(all: number): Padding;
export function padding(vertical: number, horizontal: number): Padding;
export function padding(
  top: number,
  right: number,
  bottom: number,
  left: number,
): Padding;
export function padding(...args: number[]): Padding {
  if (args.length === 1) {
    return { left: args[0], right: args[0], top: args[0], bottom: args[0] };
  }
  if (args.length === 2) {
    return { left: args[1], right: args[1], top: args[0], bottom: args[0] };
  }
  const [top, right, bottom, left] = args;
  return { left, right, top, bottom };
}

export interface CornerRadius {
  topLeft: number;
  topRight: number;
  bottomLeft: number;
  bottomRight: number;
}

export function cornerRadius(all: number): CornerRadius;
export function cornerRadius(
  topLeft: number,
  topRight: number,
  bottomLeft: number,
  bottomRight: number,
): CornerRadius;
export function cornerRadius(...args: number[]): CornerRadius {
  if (args.length === 1) {
    return {
      topLeft: args[0],
      topRight: args[0],
      bottomLeft: args[0],
      bottomRight: args[0],
    };
  }
  const [topLeft, topRight, bottomLeft, bottomRight] = args;
  return { topLeft, topRight, bottomLeft, bottomRight };
}

const ROUNDING_MODE = { styleDefault: 0, none: 1, fixed: 2 } as const;

export interface NativeRounding {
  mode: number;
  radius: CornerRadius;
}

export interface RoundingConstructors {
  styleDefault(): NativeRounding;
  none(): NativeRounding;
  fixed(topLeft: number, topRight: number, bottomLeft: number, bottomRight: number): NativeRounding;
}

export const Rounding: RoundingConstructors = {
  styleDefault(): NativeRounding {
    return { mode: ROUNDING_MODE.styleDefault, radius: cornerRadius(0) };
  },
  none(): NativeRounding {
    return { mode: ROUNDING_MODE.none, radius: cornerRadius(0) };
  },
  fixed(
    topLeft: number,
    topRight: number,
    bottomLeft: number,
    bottomRight: number,
  ): NativeRounding {
    return {
      mode: ROUNDING_MODE.fixed,
      radius: cornerRadius(topLeft, topRight, bottomLeft, bottomRight),
    };
  },
};
