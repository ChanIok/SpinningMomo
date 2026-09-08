// 从 GitHub 最新 release 的便携包中提取 Android 捕获服务 jar，供未配置 Android 开发环境的开发者使用。
// 下载完成后在 build/android/ 下写入 skip 标记，build:android 检测到后会跳过编译。

const fs = require("node:fs");
const path = require("node:path");
const os = require("node:os");
const { execFileSync } = require("node:child_process");

const projectDir = path.resolve(__dirname, "..");
const REPO = "ChanIok/SpinningMomo";
const ANDROID_DIR = path.join(projectDir, "build", "android");
const JAR_PATH = path.join(ANDROID_DIR, "momo-capture.jar");
const MARKER_PATH = path.join(ANDROID_DIR, ".android-jar-fetched");
const JAR_ENTRY = "resources/android/momo-capture.jar";

async function main() {
  console.log(`Fetching latest release info from ${REPO}...`);
  const releaseRes = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, {
    headers: { "User-Agent": "spinning-momo-build" },
  });
  if (!releaseRes.ok) {
    throw new Error(`GitHub API error: ${releaseRes.status} ${releaseRes.statusText}`);
  }
  const release = await releaseRes.json();
  console.log(`Latest release: ${release.tag_name}`);

  const asset = release.assets.find((a) => a.name.endsWith("-Portable.zip"));
  if (!asset) {
    throw new Error("No -Portable.zip asset found in the latest release.");
  }
  console.log(`Downloading ${asset.name} (${Math.round(asset.size / 1024)} KB)...`);

  const zipRes = await fetch(asset.browser_download_url, {
    headers: { "User-Agent": "spinning-momo-build" },
  });
  if (!zipRes.ok) {
    throw new Error(`Download failed: ${zipRes.status} ${zipRes.statusText}`);
  }
  const arrayBuffer = await zipRes.arrayBuffer();
  const tempZip = path.join(os.tmpdir(), `spinning-momo-portable-${Date.now()}.zip`);
  fs.writeFileSync(tempZip, Buffer.from(arrayBuffer));

  try {
    fs.mkdirSync(ANDROID_DIR, { recursive: true });

    // 通过 .NET ZipFile 仅解压 jar 这一项，避免展开整个便携包。
    const psScript = `
      Add-Type -AssemblyName System.IO.Compression.FileSystem
      $zip = [System.IO.Compression.ZipFile]::OpenRead($env:SM_TEMP_ZIP)
      try {
        $entry = $zip.Entries | Where-Object { $_.FullName -eq $env:SM_JAR_ENTRY }
        if (-not $entry) { throw "Entry not found in archive: $env:SM_JAR_ENTRY" }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $env:SM_JAR_DEST, $true)
      } finally {
        $zip.Dispose()
      }
    `;
    execFileSync("powershell", ["-NoProfile", "-Command", psScript], {
      stdio: "inherit",
      env: { ...process.env, SM_TEMP_ZIP: tempZip, SM_JAR_DEST: JAR_PATH, SM_JAR_ENTRY: JAR_ENTRY },
    });

    fs.writeFileSync(MARKER_PATH, `${release.tag_name}\n`);
    console.log(`Android jar saved to ${path.relative(projectDir, JAR_PATH)}`);
    console.log(`Skip marker written: ${path.relative(projectDir, MARKER_PATH)}`);
    console.log("build:android will now be skipped automatically.");
  } finally {
    fs.rmSync(tempZip, { force: true });
  }
}

main().catch((error) => {
  console.error(`fetch:android-jar failed: ${error.message}`);
  process.exitCode = 1;
});
