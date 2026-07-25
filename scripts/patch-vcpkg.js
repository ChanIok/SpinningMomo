/*
 * Patch the Asio copy installed in Xmake's private vcpkg cache.
 *
 * clang-cl 21 may omit the out-of-line definition of the local awaiter used by
 * awaitable_frame_base::final_suspend() in optimized Windows builds. Giving the
 * awaiter a stable nested type keeps the same behavior while avoiding that code
 * generation path.
 *
 * Usage:
 *   node scripts/patch-vcpkg.js
 *   node scripts/patch-vcpkg.js --revert
 */

const fs = require("node:fs");
const path = require("node:path");

const PACKAGES_ROOT = path.resolve(__dirname, "..", "build", ".packages", "v");
const PACKAGE_DIR = "vcpkg_asio";
const TARGET_TRIPLETS = ["x64-windows-static-md", "x64-windows-static"];
const VERSION_FILE = "include/asio/version.hpp";
const VERSION_MARKER = "#define ASIO_VERSION 103200 // 1.32.0";
const TARGET_FILE = "include/asio/impl/awaitable.hpp";

const ORIGINAL = [
  "  // On final suspension the frame is popped from the top of the stack.",
  "  auto final_suspend() noexcept",
  "  {",
  "    struct result",
  "    {",
  "      awaitable_frame_base* this_;",
  "",
  "      bool await_ready() const noexcept",
  "      {",
  "        return false;",
  "      }",
  "",
  "      void await_suspend(coroutine_handle<void>) noexcept",
  "      {",
  "        this->this_->pop_frame();",
  "      }",
  "",
  "      void await_resume() const noexcept",
  "      {",
  "      }",
  "    };",
  "",
  "    return result{this};",
  "  }",
].join("\n");

const PATCHED = [
  "  struct final_suspend_awaiter",
  "  {",
  "    awaitable_frame_base* frame_;",
  "",
  "    bool await_ready() const noexcept",
  "    {",
  "      return false;",
  "    }",
  "",
  "    void await_suspend(coroutine_handle<void>) noexcept",
  "    {",
  "      frame_->pop_frame();",
  "    }",
  "",
  "    void await_resume() const noexcept",
  "    {",
  "    }",
  "  };",
  "",
  "  // On final suspension the frame is popped from the top of the stack.",
  "  auto final_suspend() noexcept",
  "  {",
  "    return final_suspend_awaiter{this};",
  "  }",
].join("\n");

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseMode() {
  const args = process.argv.slice(2);
  if (args.length === 0) {
    return "apply";
  }
  if (args.length === 1 && args[0] === "--revert") {
    return "revert";
  }
  fail("Usage: node scripts/patch-vcpkg.js [--revert]");
}

function normalizeSnippet(snippet, text) {
  return text.includes("\r\n") ? snippet.replace(/\n/g, "\r\n") : snippet;
}

function packageRoots() {
  const latestRoot = path.join(PACKAGES_ROOT, PACKAGE_DIR, "latest");
  if (!fs.existsSync(latestRoot)) {
    return [];
  }

  return fs
    .readdirSync(latestRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && entry.name !== "cache")
    .map((entry) => path.join(latestRoot, entry.name))
    .filter((root) => fs.existsSync(path.join(root, "vcpkg_installed")))
    .sort();
}

function replaceExactlyOnce(filePath, from, to) {
  const text = fs.readFileSync(filePath, "utf8");
  const source = normalizeSnippet(from, text);
  const replacement = normalizeSnippet(to, text);
  const occurrences = text.split(source).length - 1;

  if (occurrences !== 1) {
    fail([
      `Failed to match the expected Asio source exactly once: ${filePath}`,
      `Found ${occurrences} matches. The installed source may have changed.`,
    ].join("\n"));
  }

  fs.writeFileSync(filePath, text.replace(source, replacement), "utf8");
}

function processTriplet(packageRoot, triplet, mode) {
  const tripletRoot = path.join(packageRoot, "vcpkg_installed", triplet);
  if (!fs.existsSync(tripletRoot)) {
    return false;
  }
  const label = `${path.basename(packageRoot)} / ${triplet}`;

  const versionPath = path.join(tripletRoot, ...VERSION_FILE.split("/"));
  const version = fs.readFileSync(versionPath, "utf8");
  if (!version.includes(VERSION_MARKER)) {
    fail(`Unsupported Asio version in ${tripletRoot}; expected 1.32.0.`);
  }

  const targetPath = path.join(tripletRoot, ...TARGET_FILE.split("/"));
  const text = fs.readFileSync(targetPath, "utf8");
  const original = normalizeSnippet(ORIGINAL, text);
  const patched = normalizeSnippet(PATCHED, text);

  if (mode === "apply") {
    if (text.includes(patched)) {
      console.log(`already patched: ${label}`);
      return true;
    }
    replaceExactlyOnce(targetPath, ORIGINAL, PATCHED);
    console.log(`patched: ${label}`);
    return true;
  }

  if (text.includes(original)) {
    console.log(`already reverted: ${label}`);
    return true;
  }
  replaceExactlyOnce(targetPath, PATCHED, ORIGINAL);
  console.log(`reverted: ${label}`);
  return true;
}

function main() {
  const mode = parseMode();
  const roots = packageRoots();
  if (roots.length === 0) {
    fail(`Asio package cache not found under ${PACKAGES_ROOT}.`);
  }

  let handled = false;
  for (const packageRoot of roots) {
    for (const triplet of TARGET_TRIPLETS) {
      handled = processTriplet(packageRoot, triplet, mode) || handled;
    }
  }

  if (!handled) {
    fail("No supported Asio triplet was found.");
  }
}

main();
