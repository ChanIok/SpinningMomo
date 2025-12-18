const { execSync, spawnSync } = require("child_process");
const fs = require("fs");

// 常见的 clang-format 路径（VS2022）
const KNOWN_PATHS = [
  "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\Llvm\\x64\\bin\\clang-format.exe",
  "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\x64\\bin\\clang-format.exe",
  "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\Llvm\\x64\\bin\\clang-format.exe",
  "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\Llvm\\x64\\bin\\clang-format.exe",
];

function findClangFormat() {
  // 1. 先检查 PATH
  try {
    const result = execSync("where clang-format", {
      encoding: "utf-8",
      stdio: ["pipe", "pipe", "pipe"],
    });
    const firstPath = result.trim().split(/\r?\n/)[0];
    if (firstPath && fs.existsSync(firstPath)) {
      return firstPath;
    }
  } catch {
    // where 命令失败，继续检查已知路径
  }

  // 2. 检查已知路径
  for (const p of KNOWN_PATHS) {
    if (fs.existsSync(p)) {
      return p;
    }
  }

  return null;
}

function main() {
  const clangFormat = findClangFormat();

  if (!clangFormat) {
    console.log("⚠ clang-format not found, skipping.");
    process.exit(0);
  }

  // 获取要格式化的文件
  const args = process.argv.slice(2);
  
  // 检查是否是 --files 模式（lint-staged 传递文件列表）
  let files;
  if (args[0] === "--files") {
    files = args.slice(1);
  } else if (args.length > 0) {
    files = args;
  } else {
    // 默认 glob 模式
    files = ["src/**/*.cpp", "src/**/*.ixx", "src/**/*.h", "src/**/*.hpp"];
  }

  if (files.length === 0) {
    process.exit(0);
  }

  const result = spawnSync(`"${clangFormat}"`, ["-i", ...files], {
    stdio: "inherit",
    shell: true,
  });

  process.exit(result.status || 0);
}

main();
