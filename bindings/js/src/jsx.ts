export const Fragment = Symbol("n8v.Fragment");

export type ComponentFn = (props: Record<string, unknown>) => VNodeChild;

export interface VNode {
  type: string | ComponentFn | typeof Fragment;
  props: Record<string, unknown>;
  children: VNodeChild[];
}

export type VNodeChild =
  | VNode
  | string
  | number
  | boolean
  | null
  | undefined
  | VNodeChild[];

export function createElement(
  type: VNode["type"],
  props: Record<string, unknown> | null,
  ...children: VNodeChild[]
): VNode {
  return { type, props: props ?? {}, children };
}
