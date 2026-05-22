// Build MSI and Bundle installer locally.
// Prerequisites:
//   - .NET SDK 8.0+
//   - WiX: dotnet tool install --global wix
//   - WiX extensions:
//       wix extension add WixToolset.UI.wixext
//       wix extension add WixToolset.Util.wixext
//       wix extension add WixToolset.BootstrapperApplications.wixext

const fs = require("node:fs");
const path = require("node:path");
const { execSync, spawnSync } = require("node:child_process");

const projectDir = path.join(__dirname, "..");

function parseArgs(argv) {
  let version = "";
  let msiOnly = false;

  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === "--msi-only") {
      msiOnly = true;
    } else if (arg === "--version") {
      const next = argv[++i];
      if (!next) {
        throw new Error("--version requires a value");
      }
      version = next;
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }

  return { version, msiOnly };
}

function getVersionFromFile() {
  const versionFile = fs.readFileSync(path.join(projectDir, "version.json"), "utf8");
  const versionInfo = JSON.parse(versionFile);
  if (typeof versionInfo.version !== "string" || !/^\d+\.\d+\.\d+$/.test(versionInfo.version)) {
    throw new Error("Could not extract version from version.json");
  }
  return versionInfo.version;
}

function resolveVersion(cliVersion) {
  if (cliVersion) {
    if (!/^\d+\.\d+\.\d+$/.test(cliVersion)) {
      throw new Error(`Invalid version: ${cliVersion}. Use X.Y.Z`);
    }
    return cliVersion;
  }
  return getVersionFromFile();
}

function ensureWix() {
  try {
    execSync("wix --version", { stdio: "ignore" });
  } catch {
    console.error("WiX not found. Install with: dotnet tool install --global wix");
    process.exit(1);
  }
}

function runWixBuild(args) {
  const result = spawnSync("wix", ["build", ...args], { stdio: "inherit", cwd: projectDir });
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function main() {
  if (process.platform !== "win32") {
    console.error("build-msi.js only supports Windows.");
    process.exit(1);
  }

  const { version: cliVersion, msiOnly } = parseArgs(process.argv);
  const version = resolveVersion(cliVersion);

  console.log(`Building SpinningMomo v${version} MSI...`);
  ensureWix();

  const distDir = path.join(projectDir, "dist");
  const exePath = path.join(distDir, "SpinningMomo.exe");
  if (!fs.existsSync(exePath)) {
    console.log("Building project...");
    execSync("npm run build", { stdio: "inherit", cwd: projectDir });
  }

  const outputMsi = path.join(distDir, `SpinningMomo-${version}-x64.msi`);

  console.log("Building MSI...");
  runWixBuild([
    "-arch",
    "x64",
    "-d",
    `ProductVersion=${version}`,
    "-d",
    `ProjectDir=${projectDir}`,
    "-d",
    `DistDir=${distDir}`,
    "-ext",
    "WixToolset.UI.wixext",
    "-ext",
    "WixToolset.Util.wixext",
    "-culture",
    "en-US",
    "-loc",
    "installer/Package.en-us.wxl",
    "-out",
    outputMsi,
    "installer/Package.wxs",
  ]);

  console.log(`Success! Created: ${outputMsi}`);

  if (msiOnly) {
    return;
  }

  const outputExe = path.join(distDir, `SpinningMomo-${version}-x64-Setup.exe`);

  console.log("\nBuilding Bundle installer...");
  runWixBuild([
    "-arch",
    "x64",
    "-d",
    `ProductVersion=${version}`,
    "-d",
    `ProjectDir=${projectDir}`,
    "-d",
    `MsiPath=${outputMsi}`,
    "-ext",
    "WixToolset.Util.wixext",
    "-ext",
    "WixToolset.BootstrapperApplications.wixext",
    "-culture",
    "en-US",
    "-out",
    outputExe,
    "installer/Bundle.wxs",
  ]);

  console.log("\nSuccess! Created:");
  console.log(`  MSI:    ${outputMsi}`);
  console.log(`  Bundle: ${outputExe}`);
}

try {
  main();
} catch (err) {
  console.error(err.message || err);
  process.exit(1);
}
