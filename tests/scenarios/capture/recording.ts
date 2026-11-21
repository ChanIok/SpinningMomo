import assert from "node:assert/strict";
import { stat } from "node:fs/promises";
import { extname, join } from "node:path";

import {
  assertMp4Structure,
  assertOperationSuccess,
  findVisibleAsset,
  findFolderByPath,
  getFolderTree,
  invokeCommand,
  listDirectoryFileNames,
  readMp4VideoSampleCount,
  runScenarioWithTargetWindow,
  scanDirectory,
  waitForFileToGrow,
  waitForFileToRemainPresent,
  waitForFileToStabilize,
  waitForNewFile,
  waitForVisibleAsset,
  type ScenarioPhase,
} from "../support/support.ts";
import { isMainModule, waitUntil } from "../support/runtime.ts";
import type { TargetWindowHarness } from "../support/runtime.ts";

type OperationResult = {
  success: boolean;
  message: string;
};

const phase: ScenarioPhase<TargetWindowHarness> = {
  name: "recording",
  // 通过真实 recording.toggle 验证同一路径的分片写入、原位收尾和图库延迟入库。
  action: async (application, environment, targetWindow) => {
    const settingsResult = await application.call<OperationResult>("settings.patch", {
      patch: {
        window: {
          targetTitle: targetWindow.title,
        },
        features: {
          outputDirPath: environment.captureDirectory,
          organizeOutputByWindowTitle: true,
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

    await scanDirectory(application, environment.captureDirectory);
    const initialTree = await getFolderTree(application);
    const root = findFolderByPath(initialTree, environment.captureDirectory);
    assert.ok(root, "输出根应已在图库中");
    const recordingDirectory = join(environment.captureDirectory, targetWindow.title);
    assert.equal(findFolderByPath(initialTree, recordingDirectory), undefined);
    const existingFiles = new Set<string>();
    await invokeCommand(application, "recording.toggle");

    const recordingPath = await waitForNewFile(
      recordingDirectory,
      existingFiles,
      (fileName) => extname(fileName).toLocaleLowerCase("en-US") === ".mp4",
      "直接创建 MP4 录制文件",
    );
    const initialSize = await waitForFileToGrow(recordingPath, "分片 MP4 开始写入视频帧");
    assert.ok(initialSize > 0, `录制文件为空：${recordingPath}`);
    await waitUntil(
      "录制期间写入分片索引",
      async () => {
        await assertMp4Structure(recordingPath, "fragmented");
        return true;
      },
      { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
    );
    // 超过 complete 的 3 秒缓冲期，验证仍在录制的 in-flight 不会自行到期。
    await waitForFileToRemainPresent(recordingPath, 3_500, "维持录制超过忽略缓冲期");
    assert.equal(
      await findVisibleAsset(application, recordingPath),
      undefined,
      "实时监控不应将录制中的 MP4 提前入库",
    );
    await scanDirectory(application, environment.captureDirectory);
    assert.equal(
      await findVisibleAsset(application, recordingPath),
      undefined,
      "正在录制的 MP4 不应提前入库",
    );

    const filesBeforeStop = await listDirectoryFileNames(recordingDirectory);
    await invokeCommand(application, "recording.toggle");
    await waitUntil(
      "同一个 MP4 完成原位收尾",
      async () => {
        await assertMp4Structure(recordingPath, "regular");
        return true;
      },
      { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
    );
    const recordingSize = await waitForFileToStabilize(recordingPath, "录制文件写入完成");
    assert.ok(recordingSize > 0, `录制文件为空：${recordingPath}`);
    assert.deepEqual(
      await listDirectoryFileNames(recordingDirectory),
      filesBeforeStop,
      "停录不应改名或创建伴随文件",
    );
    const asset = await waitForVisibleAsset(application, recordingPath, "停录后录像自动入库");
    const finalTree = await getFolderTree(application);
    const folder = findFolderByPath(finalTree, recordingDirectory);
    assert.ok(folder, "新输出子目录应进入目录树");
    assert.equal(folder.parentId, root.id, "不能把新子目录建成独立图库根");
    assert.equal(asset.folderId, folder.id);
    assert.deepEqual(
      finalTree.map((item) => item.id).sort((a, b) => a - b),
      initialTree.map((item) => item.id).sort((a, b) => a - b),
      "录制不应增加监控根",
    );

    // 同一控制线程再次启停，覆盖会话清理后重新建立忽略与图库同步的路径。
    const filesBeforeRestart = new Set(await listDirectoryFileNames(recordingDirectory));
    await invokeCommand(application, "recording.toggle");
    const restartedPath = await waitForNewFile(
      recordingDirectory,
      filesBeforeRestart,
      (fileName) => extname(fileName).toLocaleLowerCase("en-US") === ".mp4",
      "再次启动录制",
    );
    await waitForFileToGrow(restartedPath, "第二段录像开始写入");
    await waitForFileToRemainPresent(restartedPath, 1_000, "保留第二段有效录像");
    assert.equal(
      await findVisibleAsset(application, restartedPath),
      undefined,
      "第二段录像同样应在停止前保持忽略",
    );
    await invokeCommand(application, "recording.toggle");
    await waitUntil(
      "第二段录像完成原位收尾",
      async () => {
        await assertMp4Structure(restartedPath, "regular");
        return true;
      },
      { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
    );
    await waitForVisibleAsset(application, restartedPath, "再次停录后录像入库");

    // 显式切到 CPU 软编并持续录制超过 10 秒看门狗阈值，验证多帧内部缓冲不会在 4 帧死锁或 10 秒误杀。
    const cpuSettingsResult = await application.call<OperationResult>("settings.patch", {
      patch: {
        features: {
          recording: {
            encoderMode: "cpu",
          },
        },
      },
    });
    assertOperationSuccess(cpuSettingsResult, "切换为 CPU 软编录制");

    const filesBeforeCpu = new Set(await listDirectoryFileNames(recordingDirectory));
    await invokeCommand(application, "recording.toggle");
    const cpuRecordingPath = await waitForNewFile(
      recordingDirectory,
      filesBeforeCpu,
      (fileName) => extname(fileName).toLocaleLowerCase("en-US") === ".mp4",
      "CPU 软编创建 MP4 录制文件",
    );
    const cpuInitialSize = await waitForFileToGrow(cpuRecordingPath, "CPU 软编开始写入视频帧");
    assert.ok(cpuInitialSize > 0, `CPU 软编录制文件为空：${cpuRecordingPath}`);
    await waitUntil(
      "CPU 软编录制期间写入分片索引",
      async () => {
        await assertMp4Structure(cpuRecordingPath, "fragmented");
        return true;
      },
      { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
    );

    // 维持录制超过 10 秒编码器看门狗周期，期间文件应保持分片态并持续增长。
    await waitForFileToRemainPresent(cpuRecordingPath, 11_000, "CPU 软编持续录制超过 10 秒");
    await assertMp4Structure(cpuRecordingPath, "fragmented");
    const cpuSizeBeforeStop = (await stat(cpuRecordingPath)).size;
    assert.ok(
      cpuSizeBeforeStop > cpuInitialSize,
      `CPU 软编持续录制期间未继续写入数据：initial=${cpuInitialSize}, beforeStop=${cpuSizeBeforeStop}`,
    );

    await invokeCommand(application, "recording.toggle");
    await waitUntil(
      "CPU 软编 MP4 完成原位收尾",
      async () => {
        await assertMp4Structure(cpuRecordingPath, "regular");
        return true;
      },
      { timeoutMs: 20_000, intervalMs: 100, retryErrors: true },
    );
    const cpuFinalSize = await waitForFileToStabilize(cpuRecordingPath, "CPU 软编文件写入完成");
    assert.ok(cpuFinalSize > 0, `CPU 软编最终文件为空：${cpuRecordingPath}`);
    const cpuVideoFrames = await readMp4VideoSampleCount(cpuRecordingPath);
    assert.ok(
      cpuVideoFrames > 30,
      `CPU 软编视频帧数异常偏低（可能卡帧/死锁）：frames=${cpuVideoFrames}, path=${cpuRecordingPath}`,
    );
    await waitForVisibleAsset(application, cpuRecordingPath, "CPU 软编停录后录像入库");
  },
};

export default phase;

if (isMainModule(import.meta.url)) {
  await runScenarioWithTargetWindow("capture/recording", phase.action);
}
