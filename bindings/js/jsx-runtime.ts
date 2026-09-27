import {
  createElement,
  Fragment as FragmentImpl,
  type VNode,
  type VNodeChild,
} from "./src/jsx.ts";

export const Fragment = FragmentImpl;

export function jsx(
  type: VNode["type"],
  props: Record<string, unknown> | null,
  _key?: unknown,
): VNode {
  const { children, ...rest } = props ?? {};
  return createElement(
    type,
    rest,
    ...(Array.isArray(children)
      ? children
      : children === undefined
      ? []
      : [children]) as VNodeChild[],
  );
}

export const jsxs = jsx;

// deno-lint-ignore no-namespace
export namespace JSX {
  export type Element = VNodeChild;

  export interface ElementChildrenAttribute {
    children: Record<string, never>;
  }

  // deno-lint-ignore no-empty-interface
  export interface IntrinsicElements {}
}
