// 仅用于 Vite dev：宿主 postMessage `EVAL_SCRIPT` 时在 iframe 内执行的整段脚本。
// 与 C++ 嵌入的 `bridgeScript` 无关；改此处后刷新地图即可验证，无需重链 native。
// 页壳逻辑（如 sidebar 自动收起）只在 `bridgeScript.js` 里执行一次，此处不再重复。

import { buildRuntimeCoreSnippet } from './runtimeCore.js'

export function buildMapDevEvalScriptFromPayload(serializedPayload) {
  return `
(() => {
  if (window.location.hostname !== 'myl.nuanpaper.com') return;
${buildRuntimeCoreSnippet()}
  const L = window.L;
  const map = window.__SPINNING_MOMO_MAP__;
  if (!L || !map) return;

  const payload = ${serializedPayload};
  mountOrUpdateMapRuntime({
    L,
    map,
    markers: Array.isArray(payload.markers) ? payload.markers : [],
    renderOptions: payload.renderOptions || {},
    runtimeOptions: payload.runtimeOptions || {},
  });
})();
`
}
