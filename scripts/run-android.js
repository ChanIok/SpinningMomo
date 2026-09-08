const fs = require("node:fs");
const path = require("node:path");
const { randomUUID } = require("node:crypto");
const { spawnSync } = require("node:child_process");
const { parseArgs } = require("node:util");

const root = path.resolve(__dirname, "..");
const config = require("../android/capture/build-config.json");

function main() {
  const { values } = parseArgs({ options: {
    adb: { type: "string" }, serial: { type: "string" }, screenshot: { type: "boolean" },
    output: { type: "string" }, help: { type: "boolean" },
  } });
  if (values.help) {
    console.log('用法：node scripts/run-android.js --adb "C:\\模拟器\\adb.exe" --serial "设备序列号"');
    console.log('或：node scripts/run-android.js --adb "C:\\模拟器\\adb.exe"'
      + ' --serial "设备序列号" --screenshot [--output 路径]');
    return;
  }
  if (!values.adb || !values.serial || /[\s\x00-\x1f]/.test(values.serial)) {
    throw new Error("请明确提供 --adb 可执行文件路径和 --serial 设备序列号。");
  }
  const adb = path.resolve(values.adb);
  const jar = path.join(root, "build", "android", "momo-capture.jar");
  for (const file of [adb, jar]) {
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
      throw new Error(`文件不存在：${file}${file === jar ? "；请先运行 pnpm run build:android" : ""}`);
    }
  }

  function run(args, timeout = 15000) {
    const result = spawnSync(adb, ["-s", values.serial, ...args], {
      cwd: root, encoding: "utf8", timeout, maxBuffer: 8 * 1024 * 1024,
      windowsHide: true, shell: false,
    });
    if (result.stderr) process.stderr.write(result.stderr);
    if (result.error) throw result.error;
    if (result.status !== 0) {
      throw new Error(`ADB 命令失败 (${result.status})：${result.stdout.trim()}`);
    }
    return result.stdout.trim();
  }

  function runBinary(args, timeout = 15000) {
    const result = spawnSync(adb, ["-s", values.serial, ...args], {
      cwd: root, encoding: null, timeout, maxBuffer: 64 * 1024 * 1024,
      windowsHide: true, shell: false,
    });
    if (result.stderr && result.stderr.length > 0) process.stderr.write(result.stderr);
    if (result.error) throw result.error;
    if (result.status !== 0) {
      const stdout = result.stdout ? result.stdout.toString("utf8").trim() : "";
      throw new Error(`ADB 命令失败 (${result.status})：${stdout}`);
    }
    return result.stdout;
  }

  if (run(["get-state"]) !== "device") throw new Error("目标 ADB 设备尚未就绪。");
  const apiText = run(["shell", "getprop", "ro.build.version.sdk"]);
  if (!/^\d+$/.test(apiText) || Number(apiText) < config.minSdk) {
    throw new Error(`诊断程序要求 API ${config.minSdk}+，设备返回：${apiText}`);
  }

  // 每次运行独占一个远端文件，避免不同工作区或实例互相覆盖。
  const remoteJar = `/data/local/tmp/momo-capture-${randomUUID()}.jar`;
  try {
    console.error(`向 ${values.serial} 推送诊断程序……`);
    run(["push", jar, remoteJar], 60000);
    if (values.screenshot) {
      const outputPath = path.resolve(values.output ?? path.join(root, "build", "android",
        "device-screenshot.jpg"));
      const jpeg = runBinary(["exec-out", "sh", "-c",
        `CLASSPATH=${remoteJar} app_process / com.spinningmomo.capture.Main screenshot --format jpeg --quality 100`],
        60000);
      if (!jpeg || jpeg.length < 4 || jpeg[0] !== 0xff || jpeg[1] !== 0xd8
          || jpeg[jpeg.length - 2] !== 0xff || jpeg[jpeg.length - 1] !== 0xd9) {
        throw new Error("Android 服务返回了无效的 JPEG 图片。");
      }
      fs.mkdirSync(path.dirname(outputPath), { recursive: true });
      fs.writeFileSync(outputPath, jpeg);
      console.log(`Android JPEG 截图已保存：${outputPath} (${jpeg.length} bytes)`);
    } else {
      if (values.output) throw new Error("--output 只能与 --screenshot 一起使用。");
      const output = run(["shell", `CLASSPATH=${remoteJar} app_process / com.spinningmomo.capture.Main info`], 60000);
      const report = JSON.parse(output);
      if (report.protocolVersion !== 1 || report.command !== "info" || !report.device
          || !Array.isArray(report.encoders)) {
        throw new Error("Android 服务返回了无法识别的诊断结果。");
      }
      // wm 是现有 ADB 显示控制依赖的工具；保留原文供核对物理尺寸和 override。
      report.displaySizeOutput = run(["shell", "wm", "size"]);
      console.log(JSON.stringify(report, null, 2));
    }
  } finally {
    try {
      run(["shell", "rm", "-f", remoteJar]);
    } catch (error) {
      console.error(`远端临时 JAR 清理失败 (${remoteJar})：${error.message}`);
    }
  }
}

try {
  main();
} catch (error) {
  console.error(`Android 诊断失败：${error.message}`);
  process.exitCode = 1;
}
