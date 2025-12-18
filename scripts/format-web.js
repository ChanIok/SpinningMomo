const { spawnSync } = require("child_process");
const path = require("path");

function main() {
  const args = process.argv.slice(2);
  
  if (args.length === 0) {
    console.log("No files to format");
    process.exit(0);
  }

  // 将绝对路径转换为相对于 web 目录的路径
  const webDir = path.join(__dirname, "..", "web");
  const relativeFiles = args.map((file) => path.relative(webDir, file));

  // 使用 web 目录下的 prettier
  const prettierPath = path.join(webDir, "node_modules", ".bin", "prettier.cmd");

  const result = spawnSync(prettierPath, ["--write", ...relativeFiles], {
    cwd: webDir,
    stdio: "inherit",
    shell: true,
  });

  process.exit(result.status || 0);
}

main();
