import { buildSidebarSnippet } from './sidebar.js'
import { buildRuntimeCoreSnippet } from './runtimeCore.js'

export function buildMapRuntimeScriptFromPayload(serializedPayload) {
  return `
(() => {
  if (window.location.hostname !== 'myl.nuanpaper.com') return;
${buildSidebarSnippet()}
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
