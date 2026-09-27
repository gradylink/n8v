import * as api from "./api.ts";
import { Fragment, type VNode, type VNodeChild } from "./jsx.ts";
import type { IntrinsicComponent } from "./intrinsics.ts";

function flattenText(children: VNodeChild[]): string {
  let out = "";
  const walk = (c: VNodeChild) => {
    if (c === null || c === undefined || typeof c === "boolean") return;
    if (Array.isArray(c)) {
      for (const x of c) walk(x);
      return;
    }
    out += typeof c === "object" ? "" : String(c);
  };
  for (const c of children) walk(c);
  return out;
}

function renderChildren(children: VNodeChild[]): () => void {
  return () => {
    for (const c of children) render(c);
  };
}

/** Renders one VNode (or string/number/array/Fragment child) by calling into the imperative API. */
export function render(child: VNodeChild): void {
  if (child === null || child === undefined || typeof child === "boolean") {
    return;
  }
  if (Array.isArray(child)) {
    for (const c of child) render(c);
    return;
  }
  if (typeof child === "string" || typeof child === "number") {
    api.text(String(child));
    return;
  }
  renderNode(child);
}

function renderNode(node: VNode): void {
  const { type, props, children } = node;

  if (type === Fragment) {
    for (const c of children) render(c);
    return;
  }

  if (typeof type !== "function") {
    throw new Error(
      `n8v: <${
        String(type)
      }> is not a valid JSX tag - use the PascalCase components from n8v (Flex, Button, ...), not a lowercase string tag.`,
    );
  }

  const tag = (type as Partial<IntrinsicComponent<unknown>>).n8vIntrinsic;
  if (!tag) {
    render(type({ ...props, children }));
    return;
  }

  const body = renderChildren(children);

  switch (tag) {
    case "flex":
      api.flex(props as api.FlexOptions, body);
      return;
    case "panel":
      api.panel(props as api.PanelOptions, body);
      return;
    case "sidebar":
      api.sidebar(props as unknown as api.SidebarOptions, body);
      return;
    case "page":
      api.page(props as api.PageOptions, body);
      return;
    case "text":
      api.text(flattenText(children), props as api.TextOptions);
      return;
    case "button":
      api.button(flattenText(children), props as api.ButtonOptions);
      return;
    case "checkbox":
      api.checkbox(
        flattenText(children),
        props as unknown as api.CheckedOptions,
      );
      return;
    case "toggle":
      api.toggle(flattenText(children), props as unknown as api.CheckedOptions);
      return;
    case "radio":
      api.radio(flattenText(children), props as unknown as api.RadioOptions);
      return;
    case "entry":
      api.entry(props as unknown as api.EntryOptions);
      return;
    case "dropdown":
      api.dropdown(props as unknown as api.DropdownOptions);
      return;
    case "slider":
      api.slider(props as unknown as api.SliderOptions);
      return;
    case "image":
      api.image(props as api.ImageOptions);
      return;
    case "icon":
      api.icon(props as unknown as api.IconOptions);
      return;
    default:
      throw new Error(`n8v: unknown JSX intrinsic tag "${tag}"`);
  }
}

export function runJSX(
  width: number,
  height: number,
  title: string,
  app: () => VNodeChild,
): void {
  api.run(width, height, title, () => render(app()));
}
