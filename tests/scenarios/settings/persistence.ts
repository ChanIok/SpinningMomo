import assert from "node:assert/strict";

import {
  assertOperationSuccess,
  readJsonFile,
  runScenario,
} from "../support/support.ts";

type SettingsResponse = {
  window: {
    targetTitle: string;
  };
};

type PersistedSettings = {
  window?: {
    target_title?: string;
  };
};

// 通过生产 Settings RPC 修改的值应写入便携沙箱，并在重启后恢复。
await runScenario("settings/persistence", async (application, environment) => {
  const targetTitle = "scenario-settings-target";
  const patchResult = await application.call("settings.patch", {
    patch: {
      window: {
        targetTitle,
      },
    },
  });
  assertOperationSuccess(patchResult, "更新窗口设置");

  const persisted = await readJsonFile<PersistedSettings>(environment.settingsPath);
  assert.equal(persisted.window?.target_title, targetTitle, "设置没有写入 settings.json");

  const beforeRestart = await application.call<SettingsResponse>("settings.get", {});
  assert.equal(beforeRestart.window.targetTitle, targetTitle);

  await application.restart();

  const afterRestart = await application.call<SettingsResponse>("settings.get", {});
  assert.equal(afterRestart.window.targetTitle, targetTitle, "重启后设置没有恢复");
});
