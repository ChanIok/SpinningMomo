// 仅用于 Vite dev：宿主 postMessage `EVAL_SCRIPT` 时在 iframe 内执行的整段脚本。
// 与 C++ 嵌入的 `bridgeScript` 无关；改此处后刷新地图即可验证，无需重链 native。
// 页壳逻辑（如 sidebar 自动收起）只在 `bridgeScript.js` 里执行一次，此处不再重复。

import { buildRuntimeCoreSnippet } from './runtimeCore.js'
import { buildDevPolygonCollectorSnippet } from './devPolygonCollector.js'

export function buildMapDevEvalScriptFromPayload(serializedPayload) {
  return `
(() => {
  if (window.location.hostname !== 'myl.nuanpaper.com') return;
  const DEFAULT_WORLD_ID = '1.1';
  const WORLD_ID_PATTERN = /^\\d+(?:\\.\\d+)?$/;
  const normalizeOfficialCurrentWorldId = (raw) => {
    if (typeof raw !== 'string') {
      return undefined;
    }
    let s = raw.trim();
    if (!s) {
      return undefined;
    }
    if (s.length >= 2 && s.charAt(0) === '"' && s.charAt(s.length - 1) === '"') {
      s = s.slice(1, -1).trim();
    }
    if (!s || !WORLD_ID_PATTERN.test(s)) {
      return undefined;
    }
    return s;
  };

  const readOfficialCurrentWorldId = () => {
    try {
      const rawMapState = window.localStorage && window.localStorage.getItem('infinitynikkiMapState-v2');
      if (typeof rawMapState !== 'string' || rawMapState.length === 0) {
        return DEFAULT_WORLD_ID;
      }
      const parsedMapState = JSON.parse(rawMapState);
      const worldId = normalizeOfficialCurrentWorldId(
        parsedMapState && parsedMapState.state && parsedMapState.state.currentWorldId
      );
      return worldId || DEFAULT_WORLD_ID;
    } catch (e) {
      return DEFAULT_WORLD_ID;
    }
  };
${buildRuntimeCoreSnippet()}
${buildDevPolygonCollectorSnippet()}
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
  mountOrUpdateDevPolygonCollector({
    L,
    map,
  });
})();
`
}
