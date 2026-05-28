/*
 * vcpkg 依赖 patch 统一入口
 *
 * 当前脚本只支持 xmake 私有缓存布局：
 *   build/.packages/v/vcpkg_<port>/latest/<hash>/vcpkg_installed
 *
 * 当前已自动化：
 * 1. Asio
 *    当项目使用 `import <asio/...>;` 以头文件单元方式导入 Asio 时，
 *    MSVC + header unit 会让一批 namespace-scope constexpr / inline 变量
 *    在链接期丢失稳定定义，典型表现为：
 *      - `asio::use_awaitable`
 *      - `asio::this_coro::executor`
 *      - `asio::execution::allocator`
 *      - `asio::execution::context`
 *      - `asio::execution::blocking`
 *    最终报大量 `LNK2001`。
 *    这里通过 patch 把这些变量改成带外部链接语义的定义，
 *    规避 header unit 导入时的链接问题。
 *
 * 2. WIL / strsafe / memcpy_s
 *    `import <wil/com.h>;` 目前也会触发 header unit 兼容问题，但性质不同：
 *    它会先后炸在 Windows SDK / UCRT 的 `strsafe.h`、`corecrt_memcpy_s.h`
 *    这类老式 static inline 实现上，典型报错包括：
 *      - `StringValidateDestW` 的 `C2129`
 *      - `memcpy_s` 的 `C2129`
 *    这说明 WIL 的问题不只是单个头，而是其依赖的 SDK / UCRT 头链在
 *    header unit 路径下并不稳定。
 *    这里的 patch 只收敛到 WIL 自身：
 *      - 在 WIL 头链内部启用 `STRSAFE_LIB`
 *      - 内聚 `strsafe.lib` / `legacy_stdio_definitions.lib`
 *      - 在导入 `Windows.h` 时把 UCRT 的 `_CRT_MEMCPY_S_INLINE` 调整为 `inline`
 *      - 把 WIL 内部少量 `memcpy_s` 调用改成显式边界前提下的 `CopyMemory`
 *    目标是修 `import <wil/com.h>;`，而不是改整个工程的 CRT / SDK 默认行为。
 */

const fs = require("node:fs");
const path = require("node:path");

const PACKAGES_ROOT = path.resolve(__dirname, "..", "build", ".packages", "v");
const TARGET_TRIPLETS = ["x64-windows-static-md", "x64-windows-static"];

const rules = [
  {
    id: "asio",
    packageDirs: ["vcpkg_asio"],
    version: {
      file: "include/asio/version.hpp",
      expect: "#define ASIO_VERSION 103200 // 1.32.0",
    },
    files: [
      {
        file: "include/asio/detail/config.hpp",
        replacements: [
          {
            before: [
              "#if !defined(ASIO_INLINE_VARIABLE)",
              "# define ASIO_INLINE_VARIABLE",
              "#endif // !defined(ASIO_INLINE_VARIABLE)",
              "",
              "// Default alignment.",
            ].join("\n"),
            after: [
              "#if !defined(ASIO_INLINE_VARIABLE)",
              "# define ASIO_INLINE_VARIABLE",
              "#endif // !defined(ASIO_INLINE_VARIABLE)",
              "",
              "#if !defined(ASIO_INLINE_VARIABLE_EXTERNAL)",
              "# define ASIO_INLINE_VARIABLE_EXTERNAL extern __declspec(selectany) ASIO_INLINE_VARIABLE",
              "#endif // !defined(ASIO_INLINE_VARIABLE_EXTERNAL)",
              "",
              "// Default alignment.",
            ].join("\n"),
          },
        ],
      },
      ...[
        "include/asio/as_tuple.hpp",
        "include/asio/deferred.hpp",
        "include/asio/detached.hpp",
        "include/asio/execution/allocator.hpp",
        "include/asio/execution/blocking.hpp",
        "include/asio/execution/blocking_adaptation.hpp",
        "include/asio/execution/context.hpp",
        "include/asio/execution/mapping.hpp",
        "include/asio/execution/occupancy.hpp",
        "include/asio/execution/outstanding_work.hpp",
        "include/asio/execution/relationship.hpp",
        "include/asio/experimental/use_coro.hpp",
        "include/asio/experimental/use_promise.hpp",
        "include/asio/this_coro.hpp",
        "include/asio/use_awaitable.hpp",
        "include/asio/use_future.hpp",
        "include/asio/uses_executor.hpp",
      ].map((file) => ({
        file,
        replacements: buildAsioInlineVariableReplacements(file),
      })),
    ],
  },
  {
    id: "wil",
    packageDirs: ["vcpkg_wil", "vcpkg_webview2"],
    version: {
      file: "share/wil/vcpkg.spdx.json",
      expect: '"versionInfo": "1.0.260126.7"',
    },
    files: [
      {
        file: "include/wil/result_macros.h",
        replacements: [
          {
            before: [
              "#if !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)",
              "#ifdef __MINGW32__",
              "#include <windows.h>",
              "#else",
              "#include <Windows.h>",
              "#endif",
              "#endif",
            ].join("\n"),
            after: [
              "#if !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)",
              "#ifndef _CRT_MEMCPY_S_INLINE",
              "#define _CRT_MEMCPY_S_INLINE inline",
              "#define __WIL_RESTORE_CRT_MEMCPY_S_INLINE",
              "#endif",
              "#ifdef __MINGW32__",
              "#include <windows.h>",
              "#else",
              "#include <Windows.h>",
              "#endif",
              "#ifdef __WIL_RESTORE_CRT_MEMCPY_S_INLINE",
              "#undef _CRT_MEMCPY_S_INLINE",
              "#undef __WIL_RESTORE_CRT_MEMCPY_S_INLINE",
              "#endif",
              "#endif",
            ].join("\n"),
          },
          {
            before: [
              "#if defined(__cplusplus) && !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)",
              "",
              "#include <strsafe.h>",
              "#include <intrin.h> // provides the _ReturnAddress() intrinsic",
              "#include <new.h>    // provides 'operator new', 'std::nothrow', etc.",
            ].join("\n"),
            after: [
              "#if defined(__cplusplus) && !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)",
              "",
              "// WIL header unit compatibility patch for MSVC:",
              "// avoid strsafe's header-inline path and avoid UCRT memcpy_s header-inline path.",
              "#ifndef STRSAFE_LIB",
              "#define STRSAFE_LIB",
              "#endif",
              '#pragma comment(lib, "legacy_stdio_definitions.lib")',
              "#include <strsafe.h>",
              '#pragma comment(lib, "strsafe.lib")',
              "#include <intrin.h> // provides the _ReturnAddress() intrinsic",
              "#include <new.h>    // provides 'operator new', 'std::nothrow', etc.",
            ].join("\n"),
          },
          {
            before:
              "                memcpy_s(pCopyRefCount + 1, cbData, pData, cbData); // +1 to advance past sizeof(long) counter",
            after:
              "                CopyMemory(pCopyRefCount + 1, pData, cbData); // +1 to advance past sizeof(long) counter",
          },
          {
            before: "        memcpy_s(pStart, bufferSize, pszString, stringSize);",
            after: "        CopyMemory(pStart, pszString, stringSize);",
          },
        ],
      },
      {
        file: "include/wil/result.h",
        replacements: [
          {
            before:
              "                        memcpy_s(callContextString, remainingBytes, context.contextName, copyBytes);",
            after:
              "                        CopyMemory(callContextString, context.contextName, copyBytes);",
          },
        ],
      },
      {
        file: "include/wil/resource.h",
        replacements: [
          {
            before: "            memcpy_s(result, allocatedBytes, source, bytesToCopy);",
            after: "            CopyMemory(result, source, bytesToCopy);",
          },
          {
            before:
              "            memcpy_s(result, allocSizeBytes, source, allocSizeBytes - sizeof(*source));",
            after:
              "            CopyMemory(result, source, allocSizeBytes - sizeof(*source));",
          },
        ],
      },
    ],
  },
];

function buildAsioInlineVariableReplacements(file) {
  const linesByFile = {
    "include/asio/as_tuple.hpp": [
      "ASIO_INLINE_VARIABLE constexpr partial_as_tuple as_tuple;",
    ],
    "include/asio/deferred.hpp": [
      "ASIO_INLINE_VARIABLE constexpr deferred_t deferred;",
    ],
    "include/asio/detached.hpp": [
      "ASIO_INLINE_VARIABLE constexpr detached_t detached;",
    ],
    "include/asio/execution/allocator.hpp": [
      "ASIO_INLINE_VARIABLE constexpr allocator_t<void> allocator;",
    ],
    "include/asio/execution/blocking.hpp": [
      "ASIO_INLINE_VARIABLE constexpr blocking_t blocking;",
    ],
    "include/asio/execution/blocking_adaptation.hpp": [
      "ASIO_INLINE_VARIABLE constexpr blocking_adaptation_t blocking_adaptation;",
    ],
    "include/asio/execution/context.hpp": [
      "ASIO_INLINE_VARIABLE constexpr context_t context;",
    ],
    "include/asio/execution/mapping.hpp": [
      "ASIO_INLINE_VARIABLE constexpr mapping_t mapping;",
    ],
    "include/asio/execution/occupancy.hpp": [
      "ASIO_INLINE_VARIABLE constexpr occupancy_t occupancy;",
    ],
    "include/asio/execution/outstanding_work.hpp": [
      "ASIO_INLINE_VARIABLE constexpr outstanding_work_t outstanding_work;",
    ],
    "include/asio/execution/relationship.hpp": [
      "ASIO_INLINE_VARIABLE constexpr relationship_t relationship;",
    ],
    "include/asio/experimental/use_coro.hpp": [
      "ASIO_INLINE_VARIABLE constexpr use_coro_t<> use_coro;",
      "ASIO_INLINE_VARIABLE constexpr use_coro_t<> use_coro(0, 0, 0);",
    ],
    "include/asio/experimental/use_promise.hpp": [
      "ASIO_INLINE_VARIABLE constexpr use_promise_t<> use_promise;",
    ],
    "include/asio/this_coro.hpp": [
      "ASIO_INLINE_VARIABLE constexpr executor_t executor;",
      "ASIO_INLINE_VARIABLE constexpr cancellation_state_t cancellation_state;",
    ],
    "include/asio/use_awaitable.hpp": [
      "ASIO_INLINE_VARIABLE constexpr use_awaitable_t<> use_awaitable;",
      "ASIO_INLINE_VARIABLE constexpr use_awaitable_t<> use_awaitable(0, 0, 0);",
    ],
    "include/asio/use_future.hpp": [
      "ASIO_INLINE_VARIABLE constexpr use_future_t<> use_future;",
    ],
    "include/asio/uses_executor.hpp": [
      "ASIO_INLINE_VARIABLE constexpr executor_arg_t executor_arg;",
    ],
  };

  return linesByFile[file].map((line) => ({
    before: line,
    after: line.replace("ASIO_INLINE_VARIABLE", "ASIO_INLINE_VARIABLE_EXTERNAL"),
  }));
}

function fail(message) {
  console.error(message);
  process.exit(1);
}

function readText(file) {
  return fs.readFileSync(file, "utf8");
}

function writeText(file, text) {
  fs.writeFileSync(file, text, "utf8");
}

function detectEol(text) {
  return text.includes("\r\n") ? "\r\n" : "\n";
}

function normalizeSnippet(snippet, eol) {
  return snippet.replace(/\n/g, eol);
}

function toPath(root, relativePath) {
  return path.join(root, ...relativePath.split("/"));
}

function ensurePackagesRoot() {
  if (!fs.existsSync(PACKAGES_ROOT)) {
    fail(`xmake private package directory not found: ${PACKAGES_ROOT}`);
  }
}

function getPackageRoots(packageDir) {
  const latestRoot = path.join(PACKAGES_ROOT, packageDir, "latest");
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

function applyFileRule(filePath, replacements) {
  const original = readText(filePath);
  const eol = detectEol(original);
  let current = original;
  let changed = false;

  for (const replacement of replacements) {
    const before = normalizeSnippet(replacement.before, eol);
    const after = normalizeSnippet(replacement.after, eol);

    if (current.includes(after) && !current.includes(before)) {
      continue;
    }

    if (current.includes(before)) {
      current = current.replace(before, after);
      changed = true;
      continue;
    }

    fail([
      `Failed to match replacement rule: ${filePath}`,
      "Upstream content may have changed; update the script rules.",
      "Neither 'before' nor 'after' was found.",
    ].join("\n"));
  }

  if (changed) {
    writeText(filePath, current);
  }

  return changed;
}

function applyRuleToTriplet(rule, packageDir, packageRoot, triplet) {
  const tripletRoot = path.join(packageRoot, "vcpkg_installed", triplet);
  if (!fs.existsSync(tripletRoot)) {
    console.log(`skip ${rule.id} in ${packageDir} / ${triplet} / ${path.basename(packageRoot)}: triplet not installed.`);
    return false;
  }

  const versionFile = toPath(tripletRoot, rule.version.file);
  if (!fs.existsSync(versionFile)) {
    fail(`Version file not found: ${versionFile}`);
  }

  const versionText = readText(versionFile);
  if (!versionText.includes(rule.version.expect)) {
    fail(`${rule.id} in ${packageDir} / ${triplet} / ${path.basename(packageRoot)}: unexpected version, aborting.`);
  }

  let changed = false;
  for (const fileRule of rule.files) {
    const filePath = toPath(tripletRoot, fileRule.file);
    if (!fs.existsSync(filePath)) {
      fail(`File not found: ${filePath}`);
    }
    changed = applyFileRule(filePath, fileRule.replacements) || changed;
  }

  const status = changed ? "patched" : "already applied";
  console.log(`${status} ${rule.id} in ${packageDir} / ${triplet} / ${path.basename(packageRoot)}`);
  return true;
}

function main() {
  ensurePackagesRoot();

  let handledAny = false;

  for (const rule of rules) {
    for (const packageDir of rule.packageDirs) {
      const packageRoots = getPackageRoots(packageDir);
      if (packageRoots.length === 0) {
        console.log(`skip ${rule.id} in ${packageDir}: xmake private cache root not found.`);
        continue;
      }

      for (const packageRoot of packageRoots) {
        for (const triplet of TARGET_TRIPLETS) {
          handledAny = applyRuleToTriplet(rule, packageDir, packageRoot, triplet) || handledAny;
        }
      }
    }
  }

  if (!handledAny) {
    fail("No xmake private vcpkg installation directory to process.");
  }

  console.log("");
  console.log("vcpkg patch complete.");
  console.log(`packages root: ${PACKAGES_ROOT}`);
}

main();
