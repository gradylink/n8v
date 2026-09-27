import { createRequire } from "node:module";

export interface NativeRef<T> {
  value: T;
}

export interface NativeAddon {
  BoolRef: new (initial?: boolean) => NativeRef<boolean>;
  IntRef: new (initial?: number) => NativeRef<number>;
  FloatRef: new (initial?: number) => NativeRef<number>;
  StringRef: new (initial?: string) => NativeRef<string>;

  initialize(width: number, height: number, title: string): boolean;
  pumpEvents(): boolean;
  shutdown(): void;
  setStyleFamily(family: number): void;
  activeStyleFamily(): number;
  beginFrame(): void;
  endFrame(): void;

  openFlex(opts: object): void;
  closeFlex(): void;
  openPanel(opts: object): void;
  closePanel(): void;
  openSidebar(opts: object): void;
  closeSidebar(): void;
  openPage(opts: object): boolean;
  closePage(): void;

  text(opts: object, label: string): void;
  button(opts: object, label: string): void;
  checkbox(opts: object, label: string): void;
  toggle(opts: object, label: string): void;
  radio(opts: object, label: string): void;
  entry(opts: object): void;
  dropdown(opts: object): void;
  slider(opts: object): void;
  image(opts: object): void;
  icon(opts: object): void;
}

let cached: NativeAddon | null = null;

function platformPackageName(): string | null {
  // deno-lint-ignore no-explicit-any
  const proc = (globalThis as any).process;
  if (!proc) return null;
  const platform: string = proc.platform;
  const arch: string = proc.arch;
  const supported = new Set([
    "linux-x64",
    "linux-arm64",
    "darwin-x64",
    "darwin-arm64",
    "win32-x64",
  ]);
  const key = `${platform}-${arch}`;
  return supported.has(key) ? `n8v-${key}` : null;
}

export function loadNative(path?: string): NativeAddon {
  if (cached) return cached;
  const require = createRequire(import.meta.url);

  const candidates: string[] = [];
  if (path) candidates.push(path);
  // deno-lint-ignore no-explicit-any
  const env = (globalThis as any).process?.env ?? {};
  if (env.N8V_NATIVE_ADDON_PATH) candidates.push(env.N8V_NATIVE_ADDON_PATH);
  const platformPackage = platformPackageName();
  if (platformPackage) candidates.push(`${platformPackage}/n8v.node`);
  candidates.push(new URL("../n8v.node", import.meta.url).pathname);
  candidates.push(
    new URL("../../../build/bindings/js/native/n8v.node", import.meta.url)
      .pathname,
  );

  let lastError: unknown;
  for (const candidate of candidates) {
    try {
      cached = require(candidate) as NativeAddon;
      return cached;
    } catch (err) {
      lastError = err;
    }
  }
  throw new Error(
    `n8v: could not load the native addon (n8v.node). Tried:\n${
      candidates.map((c) => `  - ${c}`).join("\n")
    }\n` +
      (platformPackage
        ? `If you installed via npm/bun/deno, "${platformPackage}" (an optionalDependency) should have installed alongside n8v - try reinstalling, or check that platform+arch (${platformPackage}) is one n8v actually publishes a build for.\n`
        : "n8v doesn't currently publish a prebuilt binary for this platform+arch.\n") +
      "For local development: build it with `cmake -B build -DN8V_BUILD_JS_BINDING=ON && cmake --build build --target n8v_napi`, " +
      "then copy build/bindings/js/native/n8v.node next to this package (or set N8V_NATIVE_ADDON_PATH).\n" +
      `Last error: ${lastError}`,
  );
}
