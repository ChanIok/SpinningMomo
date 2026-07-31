import assert from "node:assert/strict";
import { extname } from "node:path";

import {
  assertMp4Structure,
  assertOperationSuccess,
  invokeCommand,
  listDirectoryFileNames,
  runScenarioWithTargetWindow,
  waitForFileToGrow,
  waitForFileToRemainPresent,
  waitForFileToStabilize,
  waitForNewFile,
  waitForPathToDisappear,
  type ScenarioPhase,
} from "../support/support.ts";
import { isMainModule } from "../support/runtime.ts";
import type { TargetWindowHarness } from "../support/runtime.ts";

type OperationResult = {
  success: boolean;
  message: string;
};

const phase: ScenarioPhase<TargetWindowHarness> = {
  name: "recording",
  // 通过真实 recording.toggle Command 录制共享的目标窗口，并验证 finalize/publish 流程。
  action: async (application, environment, targetWindow) => {
    const settingsResult = await application.call<OperationResult>("settings.patch", {
      patch: {
        window: {
          targetTitle: targetWindow.title,
        },
        features: {
          outputDirPath: environment.captureDirectory,
          organizeOutputByWindowTitle: false,
          recording: {
            fps: 30,
            bitrate: 4_000_000,
            quality: 60,
            qp: 23,
            rateControl: "vbr",
            encoderMode: "auto",
            codec: "h264",
            enableHdr: false,
            captureClientArea: true,
            captureCursor: false,
            autoRestartOnResize: false,
            audioSource: "none",
            audioBitrate: 128_000,
          },
        },
      },
    });
    assertOperationSuccess(settingsResult, "配置录制场景");

    const existingFiles = new Set(await listDirectoryFileNames(environment.captureDirectory));
    await invokeCommand(application, "recording.toggle");

    const workingPath = await waitForNewFile(
      environment.captureDirectory,
      existingFiles,
      (fileName) => extname(fileName) === "",
      "录制工作文件创建",
    );
    const workingSize = await waitForFileToGrow(workingPath, "录制工作文件开始写入视频帧");
    assert.ok(workingSize > 0, `录制工作文件为空：${workingPath}`);
    await waitForFileToRemainPresent(workingPath, 2_000, "维持录制至少 2 秒");

    const filesBeforeStop = new Set(await listDirectoryFileNames(environment.captureDirectory));
    await invokeCommand(application, "recording.toggle");

    const recordingPath = await waitForNewFile(
      environment.captureDirectory,
      filesBeforeStop,
      (fileName) => extname(fileName).toLocaleLowerCase("en-US") === ".mp4",
      "录制文件 finalize 并发布",
    );
    const recordingSize = await waitForFileToStabilize(recordingPath, "录制文件写入完成");
    assert.ok(recordingSize > 0, `录制文件为空：${recordingPath}`);
    await assertMp4Structure(recordingPath);
    await waitForPathToDisappear(workingPath, "录制工作文件清理");
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenarioWithTargetWindow("capture/recording", phase.action);
}
