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
      if (runtime.boundHideHoverCardOnMapMove && runtime.boundMap.off) {
        runtime.boundMap.off('movestart', runtime.boundHideHoverCardOnMapMove);
        runtime.boundMap.off('zoomstart', runtime.boundHideHoverCardOnMapMove);
        runtime.boundMap.off('resize', runtime.boundHideHoverCardOnMapMove);
      }
      if (runtime.markerLayer && runtime.markerLayer.remove) {
        runtime.markerLayer.remove();
      }
      if (runtime.clusterLayer && runtime.clusterLayer.remove) {
        runtime.clusterLayer.remove();
      }
      if (runtime.hoverCardRoot && runtime.hoverCardRoot.remove) {
        runtime.hoverCardRoot.remove();
      }
      runtime.markerLayer = null;
      runtime.clusterLayer = null;
      runtime.boundRecluster = null;
      runtime.hoverCardRoot = null;
      runtime.boundHideHoverCardOnMapMove = null;
      runtime.activeHoverCardOwner = null;
      runtime.activeHoverCardContext = null;
    }

    runtime.boundMap = map;

    if (!runtime.markerLayer) {
      runtime.markerLayer = L.layerGroup().addTo(map);
    }
    if (!runtime.clusterLayer) {
      runtime.clusterLayer = L.layerGroup().addTo(map);
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
