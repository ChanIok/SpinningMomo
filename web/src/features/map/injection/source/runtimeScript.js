import { buildSidebarSnippet } from './sidebar.js'
import { buildPaneStyleSnippet } from './paneStyle.js'
import { buildPopupSnippet } from './popup.js'
import { buildClusterSnippet } from './cluster.js'
import { buildRenderSnippet } from './render.js'

export function buildMapRuntimeScriptFromPayload(serializedPayload) {
  return `
(() => {
  if (window.location.hostname !== 'myl.nuanpaper.com') return;
${buildSidebarSnippet()}
  const L = window.L;
  const map = window.__SPINNING_MOMO_MAP__;
  if (!L || !map) return;

  const payload = ${serializedPayload};
  const markers = Array.isArray(payload.markers) ? payload.markers : [];
  const renderOptions = payload.renderOptions || {};
  const runtimeOptions = payload.runtimeOptions || {};
  const clusterEnabled = runtimeOptions.clusterEnabled !== false;
  const clusterRadius = Number(runtimeOptions.clusterRadius || 44);
  const hoverCardEnabled = runtimeOptions.hoverCardEnabled !== false;

${buildPaneStyleSnippet()}
  if (!window.__SPINNING_MOMO_RUNTIME__) {
    window.__SPINNING_MOMO_RUNTIME__ = {};
  }
  const runtime = window.__SPINNING_MOMO_RUNTIME__;

  if (!runtime.markerLayer) {
    runtime.markerLayer = L.layerGroup().addTo(map);
  }
  if (!runtime.clusterLayer) {
    runtime.clusterLayer = L.layerGroup().addTo(map);
  }
  if (!runtime.clusterHoverPopup) {
    runtime.clusterHoverPopup = L.popup({
      pane: clusterPopupPaneName,
      autoPan: true,
      autoPanPaddingTopLeft: [16, 16],
      autoPanPaddingBottomRight: [16, 16],
      closeButton: false,
      className: clusterPopupClassName,
      offset: [0, -10],
    });
  }
  if (!runtime.activeClusterPopupOwner) {
    runtime.activeClusterPopupOwner = null;
  }

  runtime.markers = markers;
  runtime.renderOptions = renderOptions;
  runtime.runtimeOptions = runtimeOptions;

${buildPopupSnippet()}
${buildClusterSnippet()}
${buildRenderSnippet()}
})();
`
}
