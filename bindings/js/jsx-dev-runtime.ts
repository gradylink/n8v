import { Fragment, jsx, jsxs } from "./jsx-runtime.ts";
import type { VNode } from "./src/jsx.ts";

export { Fragment, jsxs };

export function jsxDEV(
  type: VNode["type"],
  props: Record<string, unknown> | null,
): VNode {
  return jsx(type, props);
}
