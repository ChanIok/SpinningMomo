const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");

const root = path.resolve(__dirname, "..");
const config = require("../android/capture/build-config.json");

function requireFile(file, description) {
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
    throw new Error(`${description} not found: ${file}`);
  }
  return file;
}

function collectFiles(directory, suffix) {
  return fs.readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const file = path.join(directory, entry.name);
    return entry.isDirectory() ? collectFiles(file, suffix) : file.endsWith(suffix) ? [file] : [];
  }).sort();
}

function run(executable, args) {
  const result = spawnSync(executable, args, {
    cwd: root,
    stdio: "inherit",
    windowsHide: true,
    shell: false,
  });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${path.basename(executable)} failed (${result.status})`);
}

// Java argument file 有自己的引号规则，不经过 Windows shell。
function quoteJavaArgument(value) {
  if (/[\r\n]/.test(value)) throw new Error("Java argument must not contain newlines");
  return `"${value.replace(/\\/g, "\\\\").replace(/"/g, '\\"')}"`;
}

function main() {
  if (process.argv.length > 2) throw new Error("Usage: node scripts/build-android.js");

  // 若开发者通过 fetch:android-jar 获取了预编译 jar 并留下标记，则跳过本地编译。
  // 需要重新编译时删除 build/android/.android-jar-fetched 即可。
  const outputDirectory = path.join(root, "build", "android");
  const markerPath = path.join(outputDirectory, ".android-jar-fetched");
  const jarPath = path.join(outputDirectory, "momo-capture.jar");
  if (fs.existsSync(markerPath) && fs.existsSync(jarPath)) {
    const tag = fs.readFileSync(markerPath, "utf8").trim() || "unknown";
    console.log(`Android jar fetched from release ${tag}; skipping build:android.`);
    console.log(`Remove ${path.relative(root, markerPath)} to rebuild from source.`);
    return;
  }

  const javaHome = process.env.JAVA_HOME;
  const sdkHome = process.env.ANDROID_HOME;
  if (!javaHome) throw new Error("Please set JAVA_HOME to the JDK 21 root directory.");
  if (!sdkHome) throw new Error("Please set ANDROID_HOME to the Android SDK root directory.");
  const java = requireFile(path.join(javaHome, "bin", "java.exe"), "Java");
  const javac = requireFile(path.join(javaHome, "bin", "javac.exe"), "Java compiler");
  const androidJar = requireFile(
    path.join(sdkHome, "platforms", `android-${config.compileSdk}`, "android.jar"), "Android API jar");
  const d8Jar = requireFile(
    path.join(sdkHome, "build-tools", config.buildTools, "lib", "d8.jar"), "D8 compiler");
  const sources = collectFiles(path.join(root, "android", "capture", "src"), ".java");
  if (!sources.length) throw new Error("No Android Java source files found.");

  fs.mkdirSync(outputDirectory, { recursive: true });
  const workingDirectory = fs.mkdtempSync(path.join(outputDirectory, "compile-"));
  try {
    const classes = path.join(workingDirectory, "classes");
    fs.mkdirSync(classes);
    const argumentsFile = path.join(workingDirectory, "javac.args");
    const args = ["--release", String(config.javaRelease), "-encoding", "UTF-8", "-g:none",
      "-classpath", androidJar, "-d", classes, ...sources];
    fs.writeFileSync(argumentsFile, args.map(quoteJavaArgument).join("\n"), "utf8");
    run(javac, [`@${argumentsFile}`]);

    const generatedJar = path.join(workingDirectory, "momo-capture.jar");
    // 直接启动 SDK 的 Java 入口，避免 .bat 路径、空格和 shell 参数转义问题。
    run(java, ["-cp", d8Jar, "com.android.tools.r8.D8", "--release",
      "--min-api", String(config.minSdk), "--lib", androidJar,
      "--output", generatedJar, ...collectFiles(classes, ".class")]);
    requireFile(generatedJar, "DEX JAR output");
    const output = path.join(outputDirectory, "momo-capture.jar");
    fs.renameSync(generatedJar, output);
    console.log(`Android service generated: ${output} (${fs.statSync(output).size} bytes)`);
  } finally {
    // 递归清理只允许触及本次构建创建的目录。
    const resolvedRoot = fs.realpathSync(outputDirectory);
    const resolvedWork = fs.realpathSync(workingDirectory);
    const relative = path.relative(resolvedRoot, resolvedWork);
    if (!relative || relative.startsWith("..") || path.isAbsolute(relative)
        || path.dirname(relative) !== "." || !path.basename(relative).startsWith("compile-")) {
      throw new Error(`Refusing to clean out-of-bounds directory: ${resolvedWork}`);
    }
    fs.rmSync(resolvedWork, { recursive: true });
  }
}

try {
  main();
} catch (error) {
  console.error(`Android build failed: ${error.message}`);
  process.exitCode = 1;
}
