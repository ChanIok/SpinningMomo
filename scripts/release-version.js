// 运行此脚本更新版本号
// npm run release:version -- 2.0.1

const fs = require("fs");
const path = require("path");

function normalizeVersion(input) {
  const cleaned = input.startsWith("v") || input.startsWith("V") ? input.slice(1) : input;
  const isValid = /^\d+\.\d+\.\d+$/.test(cleaned);
  if (!isValid) {
    throw new Error("Invalid version. Use X.Y.Z (optionally prefixed with v).");
  }
  return cleaned;
}

function toVersion4Parts(version) {
  const parts = version.split(".").map((p) => Number.parseInt(p, 10));
  while (parts.length < 4) {
    parts.push(0);
  }
  return parts;
}

function updateVersionJson(filePath, version) {
  const updated = `${JSON.stringify({ version }, null, 2)}\n`;
  fs.writeFileSync(filePath, updated, "utf8");
}

function updateAppRc(filePath, version4Parts) {
  const content = fs.readFileSync(filePath, "utf8");
  const versionNum = version4Parts.join(", ");
  const versionStr = version4Parts.join(".");

  const hasVersionNum = /#define APP_VERSION_NUM .*/.test(content);
  const hasVersionStr = /#define APP_VERSION_STR ".*"/.test(content);

  if (!hasVersionNum || !hasVersionStr) {
    throw new Error("resources/app.rc format is unexpected. APP_VERSION_NUM / APP_VERSION_STR not found.");
  }

  const updated = content
    .replace(/#define APP_VERSION_NUM .*/, `#define APP_VERSION_NUM ${versionNum}`)
    .replace(/#define APP_VERSION_STR ".*"/, `#define APP_VERSION_STR "${versionStr}"`);

  fs.writeFileSync(filePath, updated, "utf8");
}

function updateVersionModule(filePath, version4Parts) {
  const content = fs.readFileSync(filePath, "utf8");
  const versionStr = version4Parts.join(".");
  const pattern = /export auto get_app_version\(\) -> std::string \{ return ".*"; \}/;

  if (!pattern.test(content)) {
    throw new Error("src/vendor/version.ixx format is unexpected. get_app_version() not found.");
  }

  const updated = content.replace(
    pattern,
    `export auto get_app_version() -> std::string { return "${versionStr}"; }`
  );

  fs.writeFileSync(filePath, updated, "utf8");
}

function updateVersionTxt(filePath, version) {
  fs.writeFileSync(filePath, `${version}\n`, "utf8");
}

function main() {
  const rawVersion = process.argv[2];
  if (!rawVersion) {
    console.error("Usage: npm run release:version -- <version>");
    console.error("Example: npm run release:version -- 2.1.0");
    process.exit(1);
  }

  const projectRoot = path.join(__dirname, "..");
  const versionJsonPath = path.join(projectRoot, "version.json");
  const appRcPath = path.join(projectRoot, "resources", "app.rc");
  const versionModulePath = path.join(projectRoot, "src", "vendor", "version.ixx");
  const versionTxtPath = path.join(projectRoot, "docs", "public", "version.txt");
  process.chdir(projectRoot);

  const version = normalizeVersion(rawVersion);
  const version4Parts = toVersion4Parts(version);
  const tagName = `v${version}`;

  updateVersionJson(versionJsonPath, version);
  updateAppRc(appRcPath, version4Parts);
  updateVersionModule(versionModulePath, version4Parts);
  updateVersionTxt(versionTxtPath, version);

  console.log("");
  console.log(`Version files updated for ${tagName}`);
  console.log("Updated:");
  console.log(`- version.json -> ${version}`);
  console.log(`- resources/app.rc -> ${version4Parts.join(".")}`);
  console.log(`- src/vendor/version.ixx -> ${version4Parts.join(".")}`);
  console.log(`- docs/public/version.txt -> ${version}`);
  console.log("");
  console.log("Next:");
  console.log("- Review the diff and commit it");
  console.log(`- Create tag: git tag ${tagName}`);
  console.log(`- Push branch and tag: git push origin HEAD ${tagName}`);
}

try {
  main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`Error: ${message}`);
  process.exit(1);
}
