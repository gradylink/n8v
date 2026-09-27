import type {
  ButtonOptions,
  CheckedOptions,
  DropdownOptions,
  EntryOptions,
  FlexOptions,
  IconOptions,
  ImageOptions,
  PageOptions,
  PanelOptions,
  RadioOptions,
  SidebarOptions,
  SliderOptions,
  TextOptions,
} from "./api.ts";
import type { VNodeChild } from "./jsx.ts";

export interface WithChildren {
  children?: VNodeChild | VNodeChild[];
}

export type IntrinsicComponent<P> = ((props: P) => never) & {
  n8vIntrinsic: string;
};

function intrinsic<P>(tag: string): IntrinsicComponent<P> {
  const fn = ((_props: P) => {
    throw new Error(
      `n8v: <${fn.n8vIntrinsic}> was called directly instead of through n8v's JSX runtime`,
    );
  }) as IntrinsicComponent<P>;
  fn.n8vIntrinsic = tag;
  return fn;
}

export const Flex: IntrinsicComponent<FlexOptions & WithChildren> = intrinsic(
  "flex",
);
export const Panel: IntrinsicComponent<PanelOptions & WithChildren> = intrinsic(
  "panel",
);
export const Sidebar: IntrinsicComponent<SidebarOptions & WithChildren> =
  intrinsic("sidebar");
export const Page: IntrinsicComponent<PageOptions & WithChildren> = intrinsic(
  "page",
);
export const Text: IntrinsicComponent<TextOptions & WithChildren> = intrinsic(
  "text",
);
export const Button: IntrinsicComponent<ButtonOptions & WithChildren> =
  intrinsic("button");
export const Checkbox: IntrinsicComponent<CheckedOptions & WithChildren> =
  intrinsic("checkbox");
export const Toggle: IntrinsicComponent<CheckedOptions & WithChildren> =
  intrinsic("toggle");
export const Radio: IntrinsicComponent<RadioOptions & WithChildren> = intrinsic(
  "radio",
);
export const Entry: IntrinsicComponent<EntryOptions> = intrinsic("entry");
export const Dropdown: IntrinsicComponent<DropdownOptions> = intrinsic(
  "dropdown",
);
export const Slider: IntrinsicComponent<SliderOptions> = intrinsic("slider");
export const Image: IntrinsicComponent<ImageOptions> = intrinsic("image");
export const Icon: IntrinsicComponent<IconOptions> = intrinsic("icon");
