#!/usr/bin/env node

// 本地化模块生成脚本
// 将 locales 目录下的 JSON 文件转换为 C++ 模块文件

const fs = require("fs");
const path = require("path");

// 项目根目录
const projectRoot = path.resolve(__dirname, "..");

// 输入和输出目录
const localesDir = path.join(projectRoot, "src", "locales");
const embeddedDir = path.join(projectRoot, "src", "core", "i18n", "embedded");

// 语言映射配置
const languageMappings = {
  "en-US": {
    moduleName: "Core.I18n.Embedded.EnUS",
    variableName: "en_us_json",
    comment: "English",
  },
  "zh-CN": {
    moduleName: "Core.I18n.Embedded.ZhCN",
    variableName: "zh_cn_json",
    comment: "Chinese",
  },
};

// 将语言代码转换为文件名格式 (如 zh-CN -> zh_cn)
function toFileNameFormat(langCode) {
  return langCode.toLowerCase().replace(/-/g, "_");
}

// 将语言代码转换为模块名格式 (如 en-US -> EnUS)
function toModuleNameFormat(langCode) {
  return langCode
    .split("-")
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1).toLowerCase())
    .join("");
}

// 生成 C++ 模块文件内容
function generateCppModule(
  sourceFile,
  jsonContent,
  moduleName,
  variableName,
  languageComment
) {
  const fileSize = Buffer.byteLength(jsonContent, "utf8");

  // 转义 JSON 内容中的特殊字符
  const escapedJson = jsonContent.replace(/\\/g, "\\\\").replace(/"/g, '\\"');

  return `// Auto-generated embedded ${languageComment} locale module
// DO NOT EDIT - This file contains embedded locale data
//
// Source: ${sourceFile}
// Module: ${moduleName}
// Variable: ${variableName}

module;

export module ${moduleName};

import std;

export namespace EmbeddedLocales {
    // Embedded ${languageComment} JSON content as string_view
    // Size: ${fileSize} bytes
    constexpr std::string_view ${variableName} = R"EmbeddedJson(${jsonContent})EmbeddedJson";
}
`;
}

// 处理单个语言文件
function processLanguageFile(fileName) {
  // 获取语言代码 (去除 .json 扩展名)
  const langCode = path.basename(fileName, ".json");

  // 构建文件路径
  const inputPath = path.join(localesDir, fileName);
  const outputPath = path.join(
    embeddedDir,
    `${toFileNameFormat(langCode)}.ixx`
  );

  // 读取 JSON 文件内容
  const jsonContent = fs.readFileSync(inputPath, "utf8");

  // 获取相对于项目根目录的路径用于注释
  const relativePath = path
    .relative(projectRoot, inputPath)
    .replace(/\\/g, "/");

  // 获取映射配置或使用默认配置
  const mapping = languageMappings[langCode] || {
    moduleName: `Core.I18n.Embedded.${toModuleNameFormat(langCode)}`,
    variableName: `${toFileNameFormat(langCode)}_json`,
    comment: langCode,
  };

  // 生成 C++ 模块内容
  const cppContent = generateCppModule(
    relativePath,
    jsonContent,
    mapping.moduleName,
    mapping.variableName,
    mapping.comment
  );

  // 写入输出文件
  fs.writeFileSync(outputPath, cppContent);

  console.log(
    `Generated embedded locale: ${fileName} -> ${path.basename(
      outputPath
    )} (${Buffer.byteLength(jsonContent, "utf8")} bytes)`
  );
}

// 主函数
function main() {
  console.log("Generating embedded locale modules...");

  // 确保输出目录存在
  if (!fs.existsSync(embeddedDir)) {
    fs.mkdirSync(embeddedDir, { recursive: true });
  }

  // 读取 locales 目录下的所有文件
  const files = fs.readdirSync(localesDir);

  // 过滤出 JSON 文件
  const jsonFiles = files.filter((file) => path.extname(file) === ".json");

  // 处理每个 JSON 文件
  jsonFiles.forEach(processLanguageFile);

  console.log("Successfully generated all embedded locale modules");
}

// 执行主函数
main();
