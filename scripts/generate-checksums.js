const path = require("path");
const fs = require("fs");
const crypto = require("crypto");

function calculateSHA256(filePath) {
  const fileBuffer = fs.readFileSync(filePath);
  const hashSum = crypto.createHash("sha256");
  hashSum.update(fileBuffer);
  return hashSum.digest("hex");
}

function main() {
  const distDir = path.join(__dirname, "..", "dist");

  // Find release files (Setup.exe and Portable.zip)
  const files = fs.readdirSync(distDir).filter((f) => {
    return f.endsWith("-Setup.exe") || f.endsWith("-Portable.zip");
  });

  if (files.length === 0) {
    console.error("No release files found in dist/");
    console.error("Run 'npm run build:portable' and 'npm run build:installer' first.");
    process.exit(1);
  }

  console.log("Generating SHA256 checksums...");

  const checksums = files.map((file) => {
    const filePath = path.join(distDir, file);
    const hash = calculateSHA256(filePath);
    console.log(`  ${hash}  ${file}`);
    return `${hash}  ${file}`;
  });

  const outputPath = path.join(distDir, "SHA256SUMS.txt");
  fs.writeFileSync(outputPath, checksums.join("\n") + "\n", "utf8");

  console.log(`\nDone! Created: ${outputPath}`);
}

main();
