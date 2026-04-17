import { buildPaneStyleSnippet } from './paneStyle.js'
import { buildPopupSnippet } from './popup.js'
import { buildClusterSnippet } from './cluster.js'
import { buildRenderSnippet } from './render.js'
import { buildToolbarSnippet } from './toolbar.js'

export function buildRuntimeCoreSnippet() {
  return `
  const mountOrUpdateMapRuntime = (payload) => {
    const L = payload && payload.L;
    const map = payload && payload.map;
    if (!L || !map) return;

    const markers = Array.isArray(payload.markers) ? payload.markers : [];
    const renderOptions = payload.renderOptions || {};
    const runtimeOptions = payload.runtimeOptions || {};
    const shouldFlyToFirst = payload.flyToFirst === true;
    const clusterEnabled = runtimeOptions.clusterEnabled !== false;
    const clusterRadius = Number(runtimeOptions.clusterRadius || 44);
    const hoverCardEnabled = runtimeOptions.hoverCardEnabled !== false;

${buildPaneStyleSnippet()}
    if (!window.__SPINNING_MOMO_RUNTIME__) {
      window.__SPINNING_MOMO_RUNTIME__ = {};
    }
    const runtime = window.__SPINNING_MOMO_RUNTIME__;

    if (runtime.boundMap && runtime.boundMap !== map) {
      if (runtime.boundRecluster && runtime.boundMap.off) {
        runtime.boundMap.off('zoomend', runtime.boundRecluster);
        runtime.boundMap.off('moveend', runtime.boundRecluster);
      }
      if (runtime.markerLayer && runtime.markerLayer.remove) {
        runtime.markerLayer.remove();
      }
      if (runtime.clusterLayer && runtime.clusterLayer.remove) {
        runtime.clusterLayer.remove();
      }
      if (runtime.clusterHoverPopup && runtime.boundMap.closePopup) {
        runtime.boundMap.closePopup(runtime.clusterHoverPopup);
      }
      runtime.markerLayer = null;
      runtime.clusterLayer = null;
      runtime.clusterHoverPopup = null;
      runtime.boundRecluster = null;
      runtime.activeClusterPopupOwner = null;
    }

    runtime.boundMap = map;

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
${buildToolbarSnippet()}
${buildRenderSnippet()}
  };
`
}
