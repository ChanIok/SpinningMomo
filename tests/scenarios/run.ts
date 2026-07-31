import { spawn } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { REPOSITORY_ROOT } from "./support/runtime.ts";

const SCENARIO_FILES = [
  "gallery/missing_restore.ts",
  "gallery/hash_inheritance.ts",
  "gallery/scanner_metadata.ts",
  "gallery/watcher_consistency.ts",
  "gallery/expired_missing_purge.ts",
  "settings/persistence.ts",
  "gallery/folder_tree.ts",
  "gallery/move_consistency.ts",
  "gallery/path_encoding.ts",
  "gallery/unreachable_root.ts",
  "capture/screenshot.ts",
  "capture/recording.ts",
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
for (const scenarioFile of SCENARIO_FILES) {
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
