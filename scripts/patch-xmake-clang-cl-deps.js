/*
 * Temporary xmake patch for clang-cl dependency file parsing.
 *
 * clang-cl emits GCC-style dependency files when xmake enables -MMD/-MF.
 * The GCC dependency parser expects line continuations to be removed before
 * parsing, but clang_cl.lua reads the file without the continuation option.
 * That leaves a standalone `\\` token which becomes the current drive root on
 * Windows and can spuriously invalidate the PCH.
 */

const fs = require("node:fs");
const path = require("node:path");
const cp = require("node:child_process");

const BEFORE = [
  "                dependinfo.depfiles_format = depfile_format",
  "                dependinfo.depfiles = io.readfile(depfile)",
].join("\n");

const AFTER = [
  "                dependinfo.depfiles_format = depfile_format",
  '                dependinfo.depfiles = io.readfile(depfile, {continuation = "\\\\"})',
].join("\n");

function fail(message) {
  console.error(message);
  process.exit(1);
}

function getXmakeExe() {
  const command = process.platform === "win32" ? "where" : "which";

  try {
    const output = cp.execFileSync(command, ["xmake"], { encoding: "utf8" }).trim();
    const firstLine = output.split(/\r?\n/).find(Boolean);
    if (firstLine) {
      return firstLine;
    }
  } catch (error) {
    fail(`xmake executable not found: ${error.message}`);
  }

  fail("xmake executable not found.");
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
    "clang_cl.lua",
  );

  if (!fs.existsSync(targetFile)) {
    fail(`target file not found: ${targetFile}`);
  }

  const original = fs.readFileSync(targetFile, "utf8");
  const normalized = original.replace(/\r\n/g, "\n");
  const hasBefore = normalized.includes(BEFORE);
  const hasAfter = normalized.includes(AFTER);

  if (hasAfter && !hasBefore) {
    console.log(`already applied clang-cl dependency patch: ${targetFile}`);
    return;
  }

  if (hasBefore && hasAfter) {
    fail(`ambiguous clang-cl dependency patch state: ${targetFile}`);
  }

  if (!hasBefore) {
    fail([
      `failed to match clang-cl dependency patch rules: ${targetFile}`,
      "This usually means the installed xmake version has changed, or already includes a different upstream fix.",
    ].join("\n"));
  }

  const occurrences = normalized.split(BEFORE).length - 1;
  if (occurrences !== 1) {
    fail(`expected one clang-cl dependency patch location, found ${occurrences}: ${targetFile}`);
  }

  const patched = normalized.replace(BEFORE, AFTER);
  const eol = detectEol(original);
  fs.writeFileSync(targetFile, patched.replace(/\n/g, eol), "utf8");
  console.log(`patched clang-cl dependency parsing: ${targetFile}`);
}

main();
