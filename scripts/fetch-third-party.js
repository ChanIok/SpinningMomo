const fs = require("node:fs");
const path = require("node:path");
const { execSync } = require("node:child_process");

const projectDir = path.join(__dirname, "..");

function run(command, cwd = projectDir) {
  execSync(command, { stdio: "inherit", cwd });
}

function main() {
  const thirdPartyDir = path.join(projectDir, "third_party");
  fs.mkdirSync(thirdPartyDir, { recursive: true });

  const dkmHeader = path.join(thirdPartyDir, "dkm", "include", "dkm.hpp");
  if (fs.existsSync(dkmHeader)) {
    console.log("DKM already exists, skip.");
  } else {
    console.log("Cloning DKM...");
    run("git clone --depth 1 https://github.com/genbattle/dkm.git third_party/dkm");
  }

  const libUltraHdrPath = path.join(thirdPartyDir, "libultrahdr");
  const libUltraHdrHeader = path.join(libUltraHdrPath, "ultrahdr_api.h");
  const libUltraHdrRef = "v1.4.0";

  if (fs.existsSync(libUltraHdrHeader)) {
    console.log(`libultrahdr already exists, syncing to ${libUltraHdrRef}...`);
    if (fs.existsSync(path.join(libUltraHdrPath, ".git"))) {
      run(`git fetch origin tag ${libUltraHdrRef} --depth 1`, libUltraHdrPath);
      run(`git checkout ${libUltraHdrRef}`, libUltraHdrPath);
    } else {
      console.warn(
        `libultrahdr present but not a git clone; skip version sync. Remove the folder to re-fetch ${libUltraHdrRef}.`
      );
    }
  } else {
    console.log(`Cloning libultrahdr ${libUltraHdrRef}...`);
    run(
      `git clone --depth 1 --branch ${libUltraHdrRef} https://github.com/google/libultrahdr.git third_party/libultrahdr`
    );
  }
}

main();
