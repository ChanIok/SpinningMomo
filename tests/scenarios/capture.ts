import screenshotPhase from "./capture/screenshot.ts";
import recordingPhase from "./capture/recording.ts";

import { runScenarioPhases } from "./support/support.ts";
import { TargetWindowHarness } from "./support/runtime.ts";

// 两个捕获阶段共享同一个目标窗口进程，只需一次窗口启动与一套截图/录制设置。
await runScenarioPhases(
  "capture",
  [screenshotPhase, recordingPhase],
  async (_application, environment) => {
    const targetWindow = new TargetWindowHarness(environment);
    await targetWindow.start();
    return {
      context: targetWindow,
      dispose: async () => await targetWindow.stop(),
    };
  },
);
