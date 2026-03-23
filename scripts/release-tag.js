const fs = require("fs");
const path = require("path");
const { execSync } = require("child_process");

function run(command, options = {}) {
  return execSync(command, {
    stdio: "pipe",
    encoding: "utf8",
    ...options,
  }).trim();
}

function runInherit(command) {
  execSync(command, { stdio: "inherit", encoding: "utf8" });
}

function normalizeVersion(input) {
  const cleaned = input.startsWith("v") || input.startsWith("V") ? input.slice(1) : input;
  const isValid = /^\d+\.\d+\.\d+(?:\.\d+)?$/.test(cleaned);
  if (!isValid) {
    throw new Error("Invalid version. Use X.Y.Z or X.Y.Z.W (optionally prefixed with v).");
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

function updateVersionHeader(filePath, version4Parts) {
  const content = fs.readFileSync(filePath, "utf8");
  const versionNum = version4Parts.join(", ");
  const versionStr = version4Parts.join(".");

  const hasVersionNum = /#define VERSION_NUM .*/.test(content);
  const hasVersionStr = /#define VERSION_STR ".*"/.test(content);

  if (!hasVersionNum || !hasVersionStr) {
    throw new Error("version.hpp format is unexpected. VERSION_NUM / VERSION_STR not found.");
  }

  const updated = content
    .replace(/#define VERSION_NUM .*/, `#define VERSION_NUM ${versionNum}`)
    .replace(/#define VERSION_STR ".*"/, `#define VERSION_STR "${versionStr}"`);

  fs.writeFileSync(filePath, updated, "utf8");
}

function updateVersionTxt(filePath, version) {
  fs.writeFileSync(filePath, `${version}\n`, "utf8");
}

function main() {
  const rawVersion = process.argv[2];
  if (!rawVersion) {
    console.error("Usage: npm run release:tag -- <version>");
    console.error("Example: npm run release:tag -- 2.1.0");
    process.exit(1);
  }

  const projectRoot = path.join(__dirname, "..");
  const versionHeaderPath = path.join(projectRoot, "src", "version.hpp");
  const versionTxtPath = path.join(projectRoot, "docs", "public", "version.txt");
  process.chdir(projectRoot);

  const version = normalizeVersion(rawVersion);
  const version4Parts = toVersion4Parts(version);
  const tagName = `v${version}`;

  const status = run("git status --porcelain");
  if (status) {
    console.error("Working tree is not clean. Commit/stash changes before running release:tag.");
    process.exit(1);
  }

  try {
    run(`git rev-parse -q --verify refs/tags/${tagName}`);
    console.error(`Tag already exists: ${tagName}`);
    process.exit(1);
  } catch {
    // Tag does not exist.
  }

  updateVersionHeader(versionHeaderPath, version4Parts);
  updateVersionTxt(versionTxtPath, version);

  const changedFiles = run("git diff --name-only");
  const hasChanges = changedFiles.trim().length > 0;

  if (hasChanges) {
    runInherit('git add "src/version.hpp" "docs/public/version.txt"');
    runInherit(`git commit -m "chore(release): ${tagName}"`);
  } else {
    console.log("No version file changes; skipping commit.");
  }

  // Lightweight tag is enough for the release workflow (tag ref is what matters).
  runInherit(`git tag ${tagName}`);

  console.log("");
  console.log(`Release prepared: ${tagName}`);
  console.log("Updated:");
  if (hasChanges) {
    console.log(`- src/version.hpp -> ${version4Parts.join(".")}`);
    console.log(`- docs/public/version.txt -> ${version}`);
  } else {
    console.log(`- (no changes) using current HEAD`);
  }
  console.log("");
  console.log(`Next: git push origin HEAD ${tagName}`);
}

try {
  main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`Error: ${message}`);
  process.exit(1);
}
