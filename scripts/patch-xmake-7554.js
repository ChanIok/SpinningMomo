/*
 * Temporary xmake patch for xmake-io/xmake#7554
 *
 * xmake v3.0.9 manifest-mode vcpkg integration still calls:
 *   vcpkg depend-info <package> ...
 * in manifest mode. That form is rejected by vcpkg.
 *
 * Upstream fix:
 *   https://github.com/xmake-io/xmake/pull/7554
 *
 * This script patches the installed xmake in-place until a released xmake
 * version includes the upstream fix.
 */

const fs = require("node:fs");
const path = require("node:path");
const cp = require("node:child_process");

const BEFORE = [
  "    -- find dependency package",
  "    -- pass features to depend-info to get the complete dependency tree",
  "    -- e.g. curl[mbedtls] needs mbedtls libraries",
  "    -- @see https://github.com/xmake-io/xmake/issues/7388",
  "    local depend_name = name",
  "    if required_features then",
  '        depend_name = name .. "[" .. table.concat(required_features, ",") .. "]"',
  "    end",
  "    local result = nil",
  '    local argv = {"depend-info", depend_name, "--sort=reverse", "--triplet=" .. triplet}',
].join("\n");

const AFTER = [
  "    -- find dependency package",
  "    -- pass features to depend-info to get the complete dependency tree",
  "    -- e.g. curl[mbedtls] needs mbedtls libraries",
  "    -- @see https://github.com/xmake-io/xmake/issues/7388",
  "    local result = nil",
  '    local argv = {"depend-info", "--sort=reverse", "--triplet=" .. triplet}',
  "    if manifest_mode then",
  "        -- in manifest mode, `vcpkg depend-info` does not accept package arguments;",
  "        -- xmake writes a manifest with only the requested package, so depend-info",
  "        -- without a package name returns exactly that package and its transitive deps.",
  "        -- @see https://github.com/xmake-io/xmake/issues/7553",
  '        table.insert(argv, 1, "--feature-flags=versions")',
  "    else",
  "        local depend_name = name",
  "        if required_features then",
  '            depend_name = name .. "[" .. table.concat(required_features, ",") .. "]"',
  "        end",
  "        table.insert(argv, 2, depend_name)",
  "    end",
].join("\n");

function fail(message) {
  console.error(message);
  process.exit(1);
}

function getXmakeExe() {
  const command = process.platform === "win32" ? "where" : "which";
  const output = cp.execFileSync(command, ["xmake"], { encoding: "utf8" }).trim();
  const firstLine = output.split(/\r?\n/).find(Boolean);
  if (!firstLine) {
    fail("xmake executable not found.");
  }
  return firstLine;
}

function detectEol(text) {
  return text.includes("\r\n") ? "\r\n" : "\n";
}

function main() {
  const xmakeExe = getXmakeExe();
  const xmakeRoot = path.dirname(xmakeExe);
  const targetFile = path.join(
    xmakeRoot,
    "modules",
    "package",
    "manager",
    "vcpkg",
    "find_package.lua",
  );

  if (!fs.existsSync(targetFile)) {
    fail(`target file not found: ${targetFile}`);
  }

  const original = fs.readFileSync(targetFile, "utf8");
  const normalized = original.replace(/\r\n/g, "\n");

  if (normalized.includes(AFTER) && !normalized.includes(BEFORE)) {
    console.log(`already applied xmake#7554: ${targetFile}`);
    return;
  }

  if (!normalized.includes(BEFORE)) {
    fail([
      `failed to match xmake#7554 patch rules: ${targetFile}`,
      "This usually means the installed xmake version has changed, or already includes a different upstream fix.",
    ].join("\n"));
  }

  const patched = normalized.replace(BEFORE, AFTER);
  const eol = detectEol(original);
  fs.writeFileSync(targetFile, patched.replace(/\n/g, eol), "utf8");
  console.log(`patched xmake#7554: ${targetFile}`);
}

main();
