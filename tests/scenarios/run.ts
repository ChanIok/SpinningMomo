import { spawn } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { REPOSITORY_ROOT } from "./support/runtime.ts";

// 每个套件在同一个真实进程与沙箱生命周期内按顺序执行多个相互独立的场景阶段。
const SCENARIO_FILES = [
  "gallery_core.ts",
  "gallery_recovery.ts",
  "capture.ts",
];

const scenarioDirectory = dirname(fileURLToPath(import.meta.url));

function runScenarioProcess(fileName: string, arguments_: string[]): Promise<number> {
  const child = spawn(process.execPath, [join(scenarioDirectory, fileName), ...arguments_], {
    cwd: REPOSITORY_ROOT,
    stdio: "inherit",
    windowsHide: false,
  });

  return new Promise<number>((resolve, reject) => {
    let settled = false;
    child.once("error", (error) => {
      if (settled) {
        return;
      }
      settled = true;
      reject(error);
    });
    child.once("exit", (code) => {
      if (settled) {
        return;
      }
      settled = true;
      resolve(code ?? 1);
    });
  });
}

let exitCode = 0;

const cliArguments = process.argv.slice(2);
const suiteFilters: string[] = [];
for (let i = 0; i < cliArguments.length; i++) {
  const argument = cliArguments[i];
  if (argument.startsWith("--exe=") || argument.startsWith("--target-exe=")) {
    continue;
  }
  if (argument === "--exe" || argument === "--target-exe") {
    i++;
    continue;
  }
  if (!argument.startsWith("-")) {
    suiteFilters.push(argument.toLocaleLowerCase("en-US"));
  }
}

const targetScenarioFiles =
  suiteFilters.length > 0
    ? SCENARIO_FILES.filter((file) =>
        suiteFilters.some((filter) => file.toLocaleLowerCase("en-US").includes(filter)),
      )
    : SCENARIO_FILES;

if (targetScenarioFiles.length === 0) {
  console.error(
    `未找到匹配的场景套件。传入筛选：${suiteFilters.join(", ")}\n可选套件列表：${SCENARIO_FILES.join(", ")}`,
  );
  process.exit(1);
}

for (const scenarioFile of targetScenarioFiles) {
  console.log(`\n===== 场景：${scenarioFile} =====`);
  try {
    exitCode = await runScenarioProcess(scenarioFile, process.argv.slice(2));
  } catch (error) {
    console.error(`无法启动场景进程：${String(error)}`);
    exitCode = 1;
  }

  if (exitCode !== 0) {
    console.error(`场景套件在 ${scenarioFile} 处停止`);
    break;
  }
}

process.exitCode = exitCode;

