import { execFileSync } from "node:child_process";
import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import * as esbuild from "esbuild";

const root = fileURLToPath(new URL("..", import.meta.url));

await esbuild.build({
  absWorkingDir: root,
  entryPoints: ["mod.ts", "jsx-runtime.ts", "jsx-dev-runtime.ts"],
  outdir: "dist",
  bundle: true,
  format: "esm",
  platform: "node",
  target: "es2022",
  logLevel: "info",
});

console.log("[build] bundled JS -> dist/");

execFileSync(
  process.platform === "win32" ? "npx.cmd" : "npx",
  ["tsc", "-p", "tsconfig.build.json"],
  { cwd: root, stdio: "inherit" },
);

console.log("[build] .d.ts -> dist/");

const rewriteDtsExtensions = (dir) => {
  for (const entry of readdirSync(dir)) {
    const full = join(dir, entry);
    if (statSync(full).isDirectory()) {
      rewriteDtsExtensions(full);
      continue;
    }
    if (!entry.endsWith(".d.ts")) continue;
    const rewritten = readFileSync(full, "utf8").replaceAll(
      /(from\s+["'][^"']+)\.ts(["'])/g,
      "$1.js$2",
    );
    writeFileSync(full, rewritten);
  }
};
rewriteDtsExtensions(join(root, "dist"));
console.log("[build] rewrote .ts -> .js in dist/**/*.d.ts imports");
