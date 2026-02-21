#!/usr/bin/env node

// HarmonyOS Sans SC 子集字体生成脚本
//
// 用法:
//   node scripts/generate-harmonyos-fonts.js
//   node scripts/generate-harmonyos-fonts.js --dry-run
//   node scripts/generate-harmonyos-fonts.js --weights=400,500,700
//   node scripts/generate-harmonyos-fonts.js --no-clean
//
// 依赖:
//   1) 已下载字体到 third_party/HarmonyOS Sans/HarmonyOS_Sans_SC
//   2) 已安装 fonttools:
//      py -m pip install fonttools brotli zopfli

const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");

const projectRoot = path.resolve(__dirname, "..");
const sourceDir = path.join(
  projectRoot,
  "third_party",
  "HarmonyOS Sans",
  "HarmonyOS_Sans_SC"
);
const localesDir = path.join(projectRoot, "web", "src", "core", "i18n", "locales");
const webSrcDir = path.join(projectRoot, "web", "src");
const outputDir = path.join(projectRoot, "web", "src", "assets", "fonts", "harmonyos-sans-sc");
const outputCharsetPath = path.join(outputDir, "charset-generated.txt");
const outputManifestPath = path.join(outputDir, "manifest.json");
const outputLicensePath = path.join(outputDir, "LICENSE.txt");

const fontByWeight = {
  100: "HarmonyOS_Sans_SC_Thin.ttf",
  300: "HarmonyOS_Sans_SC_Light.ttf",
  400: "HarmonyOS_Sans_SC_Regular.ttf",
  500: "HarmonyOS_Sans_SC_Medium.ttf",
  700: "HarmonyOS_Sans_SC_Bold.ttf",
  900: "HarmonyOS_Sans_SC_Black.ttf",
};

const defaultWeights = [400, 500, 700];

const baseAsciiChars =
  " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
const commonCjkPunctuation = "，。！？：；、（）《》【】“”‘’…—·￥";
const extraCommonChars = "\n\r\t";

const sourceCodeScanExtensions = new Set([".vue", ".ts", ".tsx", ".js", ".jsx", ".json", ".md", ".css"]);
const sourceCharRegex = /[\u3000-\u303F\u3400-\u4DBF\u4E00-\u9FFF\uF900-\uFAFF\uFF00-\uFFEF]/gu;

function toPosixPath(filePath) {
  return filePath.replace(/\\/g, "/");
}

function ensureDir(dirPath) {
  if (!fs.existsSync(dirPath)) {
    fs.mkdirSync(dirPath, { recursive: true });
  }
}

function collectStrings(value, bucket) {
  if (typeof value === "string") {
    bucket.push(value);
    return;
  }

  if (Array.isArray(value)) {
    value.forEach((item) => collectStrings(item, bucket));
    return;
  }

  if (value && typeof value === "object") {
    Object.values(value).forEach((item) => collectStrings(item, bucket));
  }
}

function walkFiles(rootDir, onFile) {
  const entries = fs.readdirSync(rootDir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(rootDir, entry.name);
    if (entry.isDirectory()) {
      walkFiles(fullPath, onFile);
    } else if (entry.isFile()) {
      onFile(fullPath);
    }
  }
}

function collectCharsFromLocales() {
  if (!fs.existsSync(localesDir)) {
    throw new Error(`Locales directory not found: ${toPosixPath(path.relative(projectRoot, localesDir))}`);
  }

  const localeFiles = fs.readdirSync(localesDir).filter((file) => file.endsWith(".json")).sort();
  if (localeFiles.length === 0) {
    throw new Error(`No locale json files found: ${toPosixPath(path.relative(projectRoot, localesDir))}`);
  }

  const chars = new Set();
  for (const file of localeFiles) {
    const filePath = path.join(localesDir, file);
    const raw = fs.readFileSync(filePath, "utf8");
    const json = JSON.parse(raw);

    const texts = [];
    collectStrings(json, texts);

    for (const text of texts) {
      for (const ch of Array.from(text)) {
        chars.add(ch);
      }
    }
  }
  return chars;
}

function collectCharsFromSource() {
  if (!fs.existsSync(webSrcDir)) {
    throw new Error(`Source directory not found: ${toPosixPath(path.relative(projectRoot, webSrcDir))}`);
  }

  const chars = new Set();
  walkFiles(webSrcDir, (filePath) => {
    const ext = path.extname(filePath).toLowerCase();
    if (!sourceCodeScanExtensions.has(ext)) return;

    const content = fs.readFileSync(filePath, "utf8");
    const matches = content.match(sourceCharRegex);
    if (!matches) return;

    for (const ch of matches) {
      chars.add(ch);
    }
  });

  return chars;
}

function buildCharset() {
  const localeChars = collectCharsFromLocales();
  const sourceChars = collectCharsFromSource();
  const merged = new Set();

  for (const ch of Array.from(baseAsciiChars)) merged.add(ch);
  for (const ch of Array.from(commonCjkPunctuation)) merged.add(ch);
  for (const ch of Array.from(extraCommonChars)) merged.add(ch);
  localeChars.forEach((ch) => merged.add(ch));
  sourceChars.forEach((ch) => merged.add(ch));

  const ordered = Array.from(merged).sort((a, b) => {
    const aCode = a.codePointAt(0) || 0;
    const bCode = b.codePointAt(0) || 0;
    return aCode - bCode;
  });

  return {
    text: ordered.join(""),
    counts: {
      merged: ordered.length,
      locale: localeChars.size,
      source: sourceChars.size,
      ascii: Array.from(new Set(Array.from(baseAsciiChars))).length,
      punctuation: Array.from(new Set(Array.from(commonCjkPunctuation))).length,
    },
  };
}

function parseArgs() {
  const args = process.argv.slice(2);
  const options = {
    dryRun: false,
    clean: true,
    weights: [...defaultWeights],
  };

  for (const arg of args) {
    if (arg === "--dry-run") {
      options.dryRun = true;
      continue;
    }

    if (arg === "--no-clean") {
      options.clean = false;
      continue;
    }

    if (arg.startsWith("--weights=")) {
      const parsed = arg
        .slice("--weights=".length)
        .split(",")
        .map((v) => Number(v.trim()))
        .filter((v) => Number.isFinite(v));

      if (parsed.length === 0) {
        throw new Error(`Invalid --weights option: ${arg}`);
      }

      options.weights = parsed;
      continue;
    }

    throw new Error(`Unknown argument: ${arg}`);
  }

  for (const weight of options.weights) {
    if (!fontByWeight[weight]) {
      const supported = Object.keys(fontByWeight).join(", ");
      throw new Error(`Unsupported weight: ${weight}. Supported weights: ${supported}`);
    }
  }

  return options;
}

function tryRun(command, args) {
  const result = spawnSync(command, args, { stdio: "pipe", encoding: "utf8", shell: false });
  return result.status === 0;
}

function detectSubsetRunner() {
  if (tryRun("pyftsubset", ["--help"])) {
    return { command: "pyftsubset", prefixArgs: [] };
  }

  if (tryRun("py", ["-m", "fontTools.subset", "--help"])) {
    return { command: "py", prefixArgs: ["-m", "fontTools.subset"] };
  }

  if (tryRun("python", ["-m", "fontTools.subset", "--help"])) {
    return { command: "python", prefixArgs: ["-m", "fontTools.subset"] };
  }

  throw new Error(
    [
      "Cannot find fontTools subset command.",
      "Please install it first:",
      "  py -m pip install fonttools brotli zopfli",
    ].join("\n")
  );
}

function buildSubsetCommand(runner, inputTtfPath, outputWoff2Path, charsetPath) {
  return [
    ...runner.prefixArgs,
    inputTtfPath,
    `--text-file=${charsetPath}`,
    "--flavor=woff2",
    "--layout-features=*",
    "--name-IDs=*",
    "--name-legacy",
    "--name-languages=*",
    "--glyph-names",
    "--symbol-cmap",
    "--legacy-cmap",
    "--notdef-glyph",
    "--notdef-outline",
    "--recommended-glyphs",
    `--output-file=${outputWoff2Path}`,
  ];
}

function fileSizeString(filePath) {
  const size = fs.statSync(filePath).size;
  const mb = size / 1024 / 1024;
  return `${mb.toFixed(2)} MB (${size} bytes)`;
}

function main() {
  const options = parseArgs();

  console.log("=".repeat(72));
  console.log("HarmonyOS Sans SC subset font generator");
  console.log("=".repeat(72));
  console.log(`Source: ${toPosixPath(path.relative(projectRoot, sourceDir))}`);
  console.log(`Output: ${toPosixPath(path.relative(projectRoot, outputDir))}`);
  console.log(`Weights: ${options.weights.join(", ")}`);
  console.log(`Dry run: ${options.dryRun ? "yes" : "no"}`);
  console.log();

  if (!fs.existsSync(sourceDir)) {
    throw new Error(`Source font directory not found: ${toPosixPath(path.relative(projectRoot, sourceDir))}`);
  }

  if (!options.dryRun) {
    if (options.clean && fs.existsSync(outputDir)) {
      fs.rmSync(outputDir, { recursive: true, force: true });
    }
    ensureDir(outputDir);
  }

  const charsetResult = buildCharset();
  if (!options.dryRun) {
    fs.writeFileSync(outputCharsetPath, charsetResult.text, "utf8");
  }
  console.log(
    `${options.dryRun ? "Planned charset" : "Generated charset"}: ${toPosixPath(path.relative(projectRoot, outputCharsetPath))}`
  );
  console.log(
    `Charset counts: merged=${charsetResult.counts.merged}, locale=${charsetResult.counts.locale}, source=${charsetResult.counts.source}`
  );
  console.log();

  const runner = options.dryRun ? { command: "pyftsubset", prefixArgs: [] } : detectSubsetRunner();
  if (!options.dryRun) {
    console.log(`Subset runner: ${runner.command} ${runner.prefixArgs.join(" ")}`.trim());
    console.log();
  }

  const outputs = [];
  for (const weight of options.weights) {
    const inputName = fontByWeight[weight];
    const inputPath = path.join(sourceDir, inputName);
    if (!fs.existsSync(inputPath)) {
      throw new Error(`Source font file not found: ${toPosixPath(path.relative(projectRoot, inputPath))}`);
    }

    const outputName = `harmonyos-sans-sc-${weight}.woff2`;
    const outputPath = path.join(outputDir, outputName);
    const args = buildSubsetCommand(runner, inputPath, outputPath, outputCharsetPath);
    const cmdText = `${runner.command} ${args.map((v) => `"${v}"`).join(" ")}`;

    console.log(`[${weight}] ${cmdText}`);
    if (!options.dryRun) {
      const result = spawnSync(runner.command, args, { stdio: "inherit", shell: false });
      if (result.status !== 0) {
        throw new Error(`Subset generation failed for weight ${weight}`);
      }
      console.log(`[${weight}] done -> ${fileSizeString(outputPath)}`);
    }
    console.log();

    outputs.push({
      weight,
      source: inputName,
      file: outputName,
      path: toPosixPath(path.relative(projectRoot, outputPath)),
      sizeBytes: options.dryRun ? 0 : fs.statSync(outputPath).size,
    });
  }

  const sourceLicensePath = path.join(sourceDir, "LICENSE.txt");
  if (fs.existsSync(sourceLicensePath) && !options.dryRun) {
    fs.copyFileSync(sourceLicensePath, outputLicensePath);
    console.log(`Copied license: ${toPosixPath(path.relative(projectRoot, outputLicensePath))}`);
  }

  const manifest = {
    family: "HarmonyOS Sans SC",
    generatedAt: new Date().toISOString(),
    sourceDir: toPosixPath(path.relative(projectRoot, sourceDir)),
    charsetFile: toPosixPath(path.relative(projectRoot, outputCharsetPath)),
    charsetCounts: charsetResult.counts,
    weights: options.weights,
    outputs,
  };

  if (!options.dryRun) {
    fs.writeFileSync(outputManifestPath, JSON.stringify(manifest, null, 2), "utf8");
  }
  console.log(`${options.dryRun ? "Planned manifest" : "Manifest"}: ${toPosixPath(path.relative(projectRoot, outputManifestPath))}`);
  console.log();
  console.log("Done.");
}

try {
  main();
} catch (error) {
  console.error();
  console.error("Error:", error.message);
  process.exit(1);
}
