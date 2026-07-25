/*
 * Temporary xmake patch for clang-cl C++23 language selection.
 *
 * xmake v3.0.9 checks the MSVC-only `-std:c++23` spelling for clang-cl.
 * clang-cl rejects it, so xmake falls back to `-std:c++latest`, which selects
 * C++26 in Clang 21. Pass the standard directly to the Clang frontend instead.
 */

const fs = require("node:fs");
const path = require("node:path");
const cp = require("node:child_process");

const BEFORE = [
  '        if self:name() == "clang_cl" then',
  '            cxx23 = {"-std:c++23", "-std:c++latest"}',
  "        end",
].join("\n");

const AFTER = [
  '        if self:name() == "clang_cl" then',
  '            cxx23 = {"-Xclang -std=c++23", "-std:c++latest"}',
  "        end",
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
    "core",
    "tools",
    "cl.lua",
  );

  if (!fs.existsSync(targetFile)) {
    fail(`target file not found: ${targetFile}`);
  }

  const original = fs.readFileSync(targetFile, "utf8");
  const normalized = original.replace(/\r\n/g, "\n");

  if (normalized.includes(AFTER) && !normalized.includes(BEFORE)) {
    console.log(`already applied clang-cl C++23 patch: ${targetFile}`);
    return;
  }

  if (!normalized.includes(BEFORE)) {
    fail([
      `failed to match clang-cl C++23 patch rules: ${targetFile}`,
      "This usually means the installed xmake version has changed, or already includes a different upstream fix.",
    ].join("\n"));
  }

  const patched = normalized.replace(BEFORE, AFTER);
  const eol = detectEol(original);
  fs.writeFileSync(targetFile, patched.replace(/\n/g, eol), "utf8");
  console.log(`patched clang-cl C++23 selection: ${targetFile}`);
}

main();
