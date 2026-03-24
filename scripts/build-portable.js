const path = require("path");
const fs = require("fs");
const { execSync } = require("child_process");

function getVersion() {
  const versionFile = fs.readFileSync(path.join(__dirname, "..", "version.json"), "utf8");
  const versionInfo = JSON.parse(versionFile);
  if (typeof versionInfo.version !== "string" || !/^\d+\.\d+\.\d+$/.test(versionInfo.version)) {
    throw new Error("Could not extract version from version.json");
  }
  return versionInfo.version;
}

function main() {
  const projectDir = path.join(__dirname, "..");
  const distDir = path.join(projectDir, "dist");

  // Verify dist directory exists with required files
  const exePath = path.join(distDir, "SpinningMomo.exe");
  const resourcesDir = path.join(distDir, "resources");
  const legalPath = path.join(distDir, "LEGAL.md");
  const licensePath = path.join(distDir, "LICENSE");

  if (!fs.existsSync(exePath)) {
    console.error("dist/SpinningMomo.exe not found. Run 'npm run build' first.");
    process.exit(1);
  }

  if (!fs.existsSync(resourcesDir)) {
    console.error("dist/resources not found. Run 'npm run build' first.");
    process.exit(1);
  }
  if (!fs.existsSync(legalPath)) {
    console.error("dist/LEGAL.md not found. Run 'npm run build' first.");
    process.exit(1);
  }
  if (!fs.existsSync(licensePath)) {
    console.error("dist/LICENSE not found. Run 'npm run build' first.");
    process.exit(1);
  }

  const version = getVersion();
  const zipName = `SpinningMomo-${version}-x64-Portable.zip`;
  const zipPath = path.join(distDir, zipName);

  console.log(`Creating portable package: ${zipName}`);

  // Create portable marker file
  const portableMarker = path.join(distDir, "portable");
  fs.writeFileSync(portableMarker, "");

  // Remove existing ZIP if present
  if (fs.existsSync(zipPath)) {
    fs.unlinkSync(zipPath);
  }

  // Create ZIP using PowerShell (Windows native, no extra dependencies)
  const filesToZip = ["SpinningMomo.exe", "resources", "LEGAL.md", "LICENSE", "portable"]
    .map((f) => `"${path.join(distDir, f)}"`)
    .join(", ");

  execSync(
    `powershell -Command "Compress-Archive -Path ${filesToZip} -DestinationPath '${zipPath}' -Force"`,
    { stdio: "inherit" }
  );

  // Clean up portable marker (only needed inside ZIP)
  fs.unlinkSync(portableMarker);

  console.log(`Done! Created: ${zipPath}`);
}

main();
