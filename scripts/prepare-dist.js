const path = require("path");
const fs = require("fs");

function main() {
  const projectDir = path.join(__dirname, "..");
  const distDir = path.join(projectDir, "dist");
  const webDist = path.join(projectDir, "web", "dist");
  const exePath = path.join(projectDir, "build", "windows", "x64", "release", "SpinningMomo.exe");
  const legalPath = path.join(projectDir, "LEGAL.md");
  const licensePath = path.join(projectDir, "LICENSE");

  if (!fs.existsSync(webDist)) {
    console.error("web/dist not found. Run 'npm run build:web' first.");
    process.exit(1);
  }

  if (!fs.existsSync(exePath)) {
    console.error("SpinningMomo.exe not found. Run 'npm run build:cpp' first.");
    process.exit(1);
  }
  if (!fs.existsSync(legalPath)) {
    console.error("LEGAL.md not found.");
    process.exit(1);
  }
  if (!fs.existsSync(licensePath)) {
    console.error("LICENSE not found.");
    process.exit(1);
  }

  console.log(`Preparing ${distDir}...`);

  if (fs.existsSync(distDir)) {
    fs.rmSync(distDir, { recursive: true, force: true });
  }
  fs.mkdirSync(distDir, { recursive: true });

  fs.copyFileSync(exePath, path.join(distDir, "SpinningMomo.exe"));
  fs.copyFileSync(legalPath, path.join(distDir, "LEGAL.md"));
  fs.copyFileSync(licensePath, path.join(distDir, "LICENSE"));
  fs.cpSync(webDist, path.join(distDir, "resources", "web"), { recursive: true });

  console.log("Done!");
}

main();
