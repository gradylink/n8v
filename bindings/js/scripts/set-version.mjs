import { readFileSync, writeFileSync } from "node:fs";

const [version, ...paths] = process.argv.slice(2);
if (!version || paths.length === 0) {
  console.error("usage: set-version.mjs <version> <package.json path>...");
  process.exit(1);
}

for (const path of paths) {
  const pkg = JSON.parse(readFileSync(path, "utf8"));
  pkg.version = version;
  if (pkg.optionalDependencies) {
    for (const name of Object.keys(pkg.optionalDependencies)) {
      pkg.optionalDependencies[name] = version;
    }
  }
  writeFileSync(path, JSON.stringify(pkg, null, 2) + "\n");
  console.log(`[set-version] ${path} -> ${version}`);
}
