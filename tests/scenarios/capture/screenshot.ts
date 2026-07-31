import assert from "node:assert/strict";
import { extname } from "node:path";

import {
  assertOperationSuccess,
  assertPngDimensions,
  invokeCommand,
  listDirectoryFileNames,
  runScenarioWithTargetWindow,
  waitForFileToStabilize,
  waitForNewFile,
} from "../support/support.ts";

type OperationResult = {
  success: boolean;
  message: string;
};

// 通过真实 screenshot.capture Command 捕获独立 Win32 测试窗口，并验证 PNG 输出尺寸。
await runScenarioWithTargetWindow(
  "capture/screenshot",
  async (application, environment, targetWindow) => {
    const settingsResult = await application.call<OperationResult>("settings.patch", {
      patch: {
        window: {
          targetTitle: targetWindow.title,
        },
        features: {
          outputDirPath: environment.captureDirectory,
          organizeOutputByWindowTitle: false,
          screenshot: {
            fileFormat: "png",
            enableHdr: false,
            captureClientArea: true,
          },
        },
      },
    });
    assertOperationSuccess(settingsResult, "配置截图场景");

    const existingFiles = new Set(await listDirectoryFileNames(environment.captureDirectory));
    await invokeCommand(application, "screenshot.capture");

    const screenshotPath = await waitForNewFile(
      environment.captureDirectory,
      existingFiles,
      (fileName) => extname(fileName).toLocaleLowerCase("en-US") === ".png",
      "截图文件落盘",
    );
    const screenshotSize = await waitForFileToStabilize(screenshotPath, "截图文件写入完成");
    assert.ok(screenshotSize > 0, `截图文件为空：${screenshotPath}`);
    await assertPngDimensions(
      screenshotPath,
      targetWindow.clientWidth,
      targetWindow.clientHeight,
    );
  },
);
