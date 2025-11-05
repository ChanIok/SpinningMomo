#!/usr/bin/env node

// SQL 迁移脚本生成器
// 将 src/migrations 目录下的 SQL 文件转换为 C++ 模块文件
//
// 用法:
//     node scripts/generate-migrations.js
//
// 功能:
//     - 读取 src/migrations/*.sql 文件
//     - 解析 SQL 文件中的版本号和描述（从注释中读取）
//     - 按分号分割 SQL 语句
//     - 生成 C++ 模块文件到 src/core/upgrade/generated/

const fs = require("fs");
const path = require("path");

// ============================================================================
// 配置
// ============================================================================

// 项目根目录
const projectRoot = path.resolve(__dirname, "..");

// 输入和输出目录
const migrationsDir = path.join(projectRoot, "src", "migrations");
const generatedDir = path.join(
  projectRoot,
  "src",
  "core",
  "migration",
  "generated"
);

// ============================================================================
// SQL 解析
// ============================================================================

/**
 * 从 SQL 文件注释中提取元数据
 *
 * 格式:
 *     -- Version: 1
 *     -- Description: Initialize database schema
 *
 * @param {string} sqlContent - SQL 文件内容
 * @returns {{version: number|null, description: string|null}}
 */
function extractMetadataFromSql(sqlContent) {
  let version = null;
  let description = null;

  const lines = sqlContent.split("\n");

  for (const line of lines) {
    const trimmed = line.trim();

    // 匹配版本号
    const versionMatch = trimmed.match(/^--\s*Version:\s*(\d+)/i);
    if (versionMatch) {
      version = parseInt(versionMatch[1], 10);
    }

    // 匹配描述
    const descMatch = trimmed.match(/^--\s*Description:\s*(.+)/i);
    if (descMatch) {
      description = descMatch[1].trim();
    }
  }

  return { version, description };
}

/**
 * 将 SQL 文件内容分割成独立的语句
 *
 * 规则:
 * - 按分号分割
 * - 忽略空语句
 * - 保留触发器等复杂语句（BEGIN...END 内的分号不分割）
 *
 * @param {string} sqlContent - SQL 文件内容
 * @returns {string[]} SQL 语句数组
 */
function splitSqlStatements(sqlContent) {
  const statements = [];
  const currentStatement = [];
  let inBeginEnd = false;

  const lines = sqlContent.split("\n");

  for (const line of lines) {
    const stripped = line.trim();

    // 跳过纯注释行和空行
    if (!stripped || stripped.startsWith("--")) {
      continue;
    }

    // 检测 BEGIN...END 块
    if (/\bBEGIN\b/i.test(stripped)) {
      inBeginEnd = true;
    }

    currentStatement.push(line);

    // 在非 BEGIN...END 块中遇到分号，表示语句结束
    if (line.includes(";") && !inBeginEnd) {
      // 提取分号之前的内容作为完整语句
      const stmt = currentStatement
        .join("\n")
        .trim()
        .replace(/;+\s*$/, "")
        .trim();
      if (stmt) {
        statements.push(stmt);
      }
      currentStatement.length = 0; // 清空数组
    }

    // 检测 END 块结束
    if (/\bEND\b/i.test(stripped)) {
      inBeginEnd = false;
      // END 语句后通常有分号，作为完整语句
      if (line.includes(";")) {
        const stmt = currentStatement
          .join("\n")
          .trim()
          .replace(/;+\s*$/, "")
          .trim();
        if (stmt) {
          statements.push(stmt);
        }
        currentStatement.length = 0;
      }
    }
  }

  // 处理最后可能剩余的语句
  if (currentStatement.length > 0) {
    const stmt = currentStatement
      .join("\n")
      .trim()
      .replace(/;+\s*$/, "")
      .trim();
    if (stmt) {
      statements.push(stmt);
    }
  }

  return statements;
}

// ============================================================================
// C++ 代码生成
// ============================================================================

/**
 * 生成 C++ 模块文件内容
 *
 * @param {string} migrationFile - 迁移文件名
 * @param {number} version - 版本号
 * @param {string} description - 描述
 * @param {string[]} sqlStatements - SQL 语句数组
 * @returns {string} C++ 模块文件内容
 */
function generateCppModule(migrationFile, version, description, sqlStatements) {
  // 模块名称格式: V001, V002, ...
  const moduleSuffix = `V${String(version).padStart(3, "0")}`;
  const moduleName = `Core.Migration.Migrations.${moduleSuffix}`;
  const namespaceName = moduleSuffix; // 命名空间名称，如 V001

  // 生成语句数组
  const statementsCode = sqlStatements.map((stmt) => {
    // 使用自定义分隔符
    let delimiter = "SQL";
    // 确保 SQL 中不包含 )SQL" 这样的序列
    while (stmt.includes(`)${delimiter}"`)) {
      delimiter += "X";
    }

    return `        R"${delimiter}(
${stmt}
        )${delimiter}"`;
  });

  const statementsArray = statementsCode.join(",\n");

  return `// Auto-generated migration module
// DO NOT EDIT - This file is generated from src/migrations/${migrationFile}
//
// Version: ${version}
// Description: ${description}
// Statements: ${sqlStatements.length}

module;

export module ${moduleName};

import std;

export namespace Core::Migration::Migrations::${namespaceName} {

// Migration metadata
constexpr int version = ${version};
constexpr std::string_view description = "${description}";

// SQL statements
constexpr std::array<std::string_view, ${sqlStatements.length}> statements = {
${statementsArray}
};

}  // namespace Core::Migration::Migrations::${namespaceName}
`;
}

// ============================================================================
// 文件处理
// ============================================================================

/**
 * 处理单个 SQL 迁移文件
 *
 * @param {string} sqlFile - SQL 文件路径
 * @returns {boolean} 成功返回 true
 */
function processMigrationFile(sqlFile) {
  const fileName = path.basename(sqlFile);
  console.log(`Processing: ${fileName}`);

  try {
    // 读取 SQL 文件
    const sqlContent = fs.readFileSync(sqlFile, "utf8");

    // 提取元数据
    const { version, description } = extractMetadataFromSql(sqlContent);

    if (version === null) {
      console.log(`  ⚠️  Warning: No version found in ${fileName}, skipping`);
      return false;
    }

    const finalDescription = description || "No description";
    if (description === null) {
      console.log(`  ⚠️  Warning: No description found in ${fileName}`);
    }

    // 分割 SQL 语句
    const statements = splitSqlStatements(sqlContent);

    if (statements.length === 0) {
      console.log(`  ⚠️  Warning: No SQL statements found in ${fileName}`);
      return false;
    }

    console.log(`  Version: ${version}`);
    console.log(`  Description: ${finalDescription}`);
    console.log(`  Statements: ${statements.length}`);

    // 生成 C++ 代码
    const cppContent = generateCppModule(
      fileName,
      version,
      finalDescription,
      statements
    );

    // 输出文件路径
    const outputFile = path.join(
      generatedDir,
      `migration_${String(version).padStart(3, "0")}.ixx`
    );
    fs.writeFileSync(outputFile, cppContent, "utf8");

    const relativePath = path
      .relative(projectRoot, outputFile)
      .replace(/\\/g, "/");
    console.log(`  ✓ Generated: ${relativePath}`);
    return true;
  } catch (error) {
    console.log(`  ✗ Error processing ${fileName}: ${error.message}`);
    console.error(error.stack);
    return false;
  }
}

/**
 * 生成索引模块，导出所有迁移
 *
 * @param {number[]} processedVersions - 已处理的版本号数组
 */
function generateIndexModule(processedVersions) {
  if (processedVersions.length === 0) {
    return;
  }

  processedVersions.sort((a, b) => a - b);

  // 生成 import 语句
  const imports = processedVersions.map((ver) => {
    return `export import Core.Migration.Migrations.V${String(ver).padStart(
      3,
      "0"
    )};`;
  });

  const importsCode = imports.join("\n");

  const indexContent = `// Auto-generated migration index
// DO NOT EDIT - This file imports all generated migration modules

module;

export module Core.Migration.Migrations;

// Import all migration modules
${importsCode}
`;

  const indexFile = path.join(generatedDir, "migrations.ixx");
  fs.writeFileSync(indexFile, indexContent, "utf8");

  const relativePath = path
    .relative(projectRoot, indexFile)
    .replace(/\\/g, "/");
  console.log(`\n✓ Generated index: ${relativePath}`);
}

// ============================================================================
// 主函数
// ============================================================================

/**
 * 主函数
 */
function main() {
  console.log("=".repeat(70));
  console.log("SQL Migration Generator");
  console.log("=".repeat(70));
  console.log();

  // 检查输入目录
  if (!fs.existsSync(migrationsDir)) {
    console.log(`✗ Error: Migrations directory not found: ${migrationsDir}`);
    process.exit(1);
  }

  // 创建输出目录
  if (!fs.existsSync(generatedDir)) {
    fs.mkdirSync(generatedDir, { recursive: true });
  }

  const relativePath = path
    .relative(projectRoot, generatedDir)
    .replace(/\\/g, "/");
  console.log(`Output directory: ${relativePath}`);
  console.log();

  // 获取所有 SQL 文件
  const allFiles = fs.readdirSync(migrationsDir);
  const sqlFiles = allFiles
    .filter((file) => path.extname(file) === ".sql")
    .map((file) => path.join(migrationsDir, file))
    .sort();

  if (sqlFiles.length === 0) {
    console.log(`✗ No SQL files found in ${migrationsDir}`);
    process.exit(1);
  }

  console.log(`Found ${sqlFiles.length} SQL file(s)`);
  console.log();

  // 处理每个 SQL 文件
  const processedVersions = [];
  let successCount = 0;

  for (const sqlFile of sqlFiles) {
    if (processMigrationFile(sqlFile)) {
      // 提取版本号
      const fileName = path.basename(sqlFile);
      const versionMatch = fileName.match(/^(\d+)_/);
      if (versionMatch) {
        processedVersions.push(parseInt(versionMatch[1], 10));
      }
      successCount++;
    }
    console.log();
  }

  // 生成索引模块
  if (processedVersions.length > 0) {
    generateIndexModule(processedVersions);
  }

  // 输出总结
  console.log("=".repeat(70));
  console.log(
    `✓ Successfully generated ${successCount}/${sqlFiles.length} migration(s)`
  );
  console.log("=".repeat(70));

  process.exit(successCount === sqlFiles.length ? 0 : 1);
}

// 执行主函数
main();
