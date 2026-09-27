# n8v for TypeScript

TypeScript bindings for [n8v](../../README.md), with full types and support for
both plain and JSX usage.

## Requirements

- n8v built from the repo root with the JS binding enabled (see below).
- Deno 2.x, Bun 1.x, or Node 18+.

## Building the native addon

From the repo root:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DN8V_EXAMPLES=OFF -DN8V_BUILD_JS_BINDING=ON
cmake --build build --target n8v_napi
cp build/bindings/js/native/n8v.node bindings/js/
```

(`deno task build:addon` from `bindings/js/` does the same thing.)

`loadNative()` (used internally by everything below) looks for `n8v.node` next
to this package first; set `N8V_NATIVE_ADDON_PATH` to point elsewhere.

## Usage

### Plain TypeScript

```ts
import * as n8v from "./mod.ts";

const name = n8v.ref("");
let clicks = 0;

n8v.run(480, 360, "hello", () => {
  n8v.flex({ direction: "vertical", gap: 12, padding: n8v.padding(20) }, () => {
    n8v.text("Hello", { bold: true });
    n8v.button("Click me", { onClick: () => clicks++ });
    n8v.entry({ value: name, placeholder: "Your name" });
  });
});
```

Run with `deno run --allow-ffi --allow-read --allow-env examples/basic.ts`,
`bun run examples/basic.ts`, or `npx tsx examples/basic.ts`.

### JSX

Point your compiler at this package's JSX runtime:

**`deno.json`**

```json
{
  "compilerOptions": { "jsx": "react-jsx", "jsxImportSource": "n8v" },
  "imports": {
    "n8v/jsx-runtime": "./jsx-runtime.ts",
    "n8v/jsx-dev-runtime": "./jsx-dev-runtime.ts"
  }
}
```

**`tsconfig.json`** (Bun and Node/tsx both read JSX settings from here, not
`deno.json`)

```json
{
  "compilerOptions": {
    "jsx": "react-jsx",
    "jsxImportSource": "n8v",
    "paths": {
      "n8v/jsx-runtime": ["./jsx-runtime.ts"],
      "n8v/jsx-dev-runtime": ["./jsx-dev-runtime.ts"]
    }
  }
}
```

(The example configs in this directory already do this - copy them into your own
project's `bindings/js` consumer if you're not working from inside this repo.)

```tsx
import { Button, Entry, Flex, ref, runJSX, Text } from "./mod.ts";

const name = ref("");
let clicks = 0;

function App() {
  return (
    <Flex direction="vertical" gap={12}>
      <Text bold>Hello</Text>
      <Button onClick={() => clicks++}>Click me</Button>
      <Entry value={name} placeholder="Your name" />
    </Flex>
  );
}

runJSX(480, 360, "hello", () => <App />);
```

Run with `deno run --allow-ffi --allow-read --allow-env examples/jsx_app.tsx`,
`bun run examples/jsx_app.tsx`, or `npx tsx examples/jsx_app.tsx`.

## State: `ref()`

Checkbox/toggle state, radio/dropdown/sidebar selection, slider values and entry
text are mutated by n8v outside of any single call (e.g. a checkbox flips when
clicked, not when your code runs), so they need a stable native memory cell, not
a plain JS variable. `ref()` is used for this, it by default infers the type
from the initial value:

```ts
const checked = n8v.ref(false); // -> Ref<boolean>
n8v.checkbox("Subscribe", { checked }); // or <Checkbox checked={checked}>Subscribe</Checkbox>
checked.value; // reflects the user's last click
```

A plain `number` is ambiguous - selection indices (radio/dropdown/sidebar) are
`int`, slider values are `float` - so `ref()` picks float. Use `Ref.int()`
explicitly for a selection index.
