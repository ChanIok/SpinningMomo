const path = require("path");
const fs = require("fs");

function main() {
  const projectDir = path.join(__dirname, "..");
  const distDir = path.join(projectDir, "dist");
  const webDist = path.join(projectDir, "web", "dist");
  const exePath = path.join(projectDir, "build", "windows", "x64", "release", "SpinningMomo.exe");

  if (!fs.existsSync(webDist)) {
    console.error("web/dist not found. Run 'npm run build:web' first.");
    process.exit(1);
  }

  if (!fs.existsSync(exePath)) {
    console.error("SpinningMomo.exe not found. Run 'npm run build:cpp' first.");
    process.exit(1);
  }

  console.log(`Preparing ${distDir}...`);

  if (fs.existsSync(distDir)) {
    fs.rmSync(distDir, { recursive: true, force: true });
  }
  fs.mkdirSync(distDir, { recursive: true });

  fs.copyFileSync(exePath, path.join(distDir, "SpinningMomo.exe"));
  fs.cpSync(webDist, path.join(distDir, "resources", "web"), { recursive: true });

  console.log("Done!");
}

main();
