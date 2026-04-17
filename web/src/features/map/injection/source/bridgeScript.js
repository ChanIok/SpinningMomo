import { buildRuntimeCoreSnippet } from './runtimeCore.js'

export function buildMapBridgeScriptTemplate() {
  return `
if (window.location.hostname === 'myl.nuanpaper.com') {
    let innerL = undefined;
    window.__SPINNING_MOMO_ALLOW_DEV_EVAL__ = __ALLOW_DEV_EVAL__;
    window.__SPINNING_MOMO_PENDING_MARKERS__ = [];
    window.__SPINNING_MOMO_RENDER_OPTIONS__ = {};
    window.__SPINNING_MOMO_CLUSTER_OPTIONS__ = {};

${buildRuntimeCoreSnippet()}

    const normalizeRenderOptions = (options) => {
        if (!options || typeof options !== 'object') {
            return {};
        }

        const normalized = { ...options };

        if (!Array.isArray(normalized.markerIconSize) || normalized.markerIconSize.length !== 2) {
            normalized.markerIconSize = undefined;
        }
        if (!Array.isArray(normalized.markerIconAnchor) || normalized.markerIconAnchor.length !== 2) {
            normalized.markerIconAnchor = undefined;
        }
        if (!Array.isArray(normalized.popupAnchor) || normalized.popupAnchor.length !== 2) {
            normalized.popupAnchor = undefined;
        }

        return normalized;
    };

    const setRenderOptions = (options) => {
        window.__SPINNING_MOMO_RENDER_OPTIONS__ = normalizeRenderOptions(options);
    };

    const setClusterOptions = (options) => {
        if (!options || typeof options !== 'object') {
            window.__SPINNING_MOMO_CLUSTER_OPTIONS__ = {};
            return;
        }
        window.__SPINNING_MOMO_CLUSTER_OPTIONS__ = { ...options };
    };

    const maybeMountRuntime = (payload = {}) => {
        const map = window.__SPINNING_MOMO_MAP__;
        const L = window.L;
        if (!map || !L || typeof mountOrUpdateMapRuntime !== 'function') {
            return;
        }

        mountOrUpdateMapRuntime({
            L,
            map,
            markers: window.__SPINNING_MOMO_PENDING_MARKERS__,
            renderOptions: window.__SPINNING_MOMO_RENDER_OPTIONS__ || {},
            runtimeOptions: window.__SPINNING_MOMO_CLUSTER_OPTIONS__ || {},
            flyToFirst: payload.flyToFirst === true,
        });
    };

    Object.defineProperty(window, 'L', {
        get: function() { return innerL; },
        set: function(val) {
            innerL = val;
            if (innerL && innerL.Map && !innerL.Map.__SPINNING_MOMO_PATCHED__) {
                const OriginalMapClass = innerL.Map;
                innerL.Map = function(...args) {
                    const mapInstance = new OriginalMapClass(...args);
                    window.__SPINNING_MOMO_MAP__ = mapInstance;
                    maybeMountRuntime();
                    return mapInstance;
                };
                innerL.Map.prototype = OriginalMapClass.prototype;
                Object.assign(innerL.Map, OriginalMapClass);
                innerL.Map.__SPINNING_MOMO_PATCHED__ = true;
            }
        },
        configurable: true,
        enumerable: true
    });

    window.addEventListener('message', (event) => {
        if (!event.data) {
            return;
        }

        if (event.data.action === 'SET_MARKERS') {
            const { markers = [] } = event.data.payload || {};
            window.__SPINNING_MOMO_PENDING_MARKERS__ = Array.isArray(markers) ? markers : [];
            maybeMountRuntime();
            return;
        }

        if (event.data.action === 'SET_RENDER_OPTIONS') {
            setRenderOptions(event.data.payload || {});
            maybeMountRuntime();
            return;
        }

        if (event.data.action === 'SET_CLUSTER_OPTIONS') {
            setClusterOptions(event.data.payload || {});
            maybeMountRuntime();
            return;
        }

        if (event.data.action === 'EVAL_SCRIPT') {
            if (!window.__SPINNING_MOMO_ALLOW_DEV_EVAL__) {
                return;
            }

            const scriptPayload = event.data.payload || {};
            if (typeof scriptPayload.script !== 'string' || scriptPayload.script.length === 0) {
                return;
            }

            try {
                const runScript = new Function(scriptPayload.script);
                runScript();
            } catch (error) {
                console.error('[SpinningMomo] Failed to eval dev script:', error);
            }
            return;
        }

        if (event.data.action === 'ADD_MARKER') {
            const marker = event.data.payload;
            if (!marker) {
                return;
            }

            const pendingMarkers = window.__SPINNING_MOMO_PENDING_MARKERS__;
            window.__SPINNING_MOMO_PENDING_MARKERS__ = [...pendingMarkers, marker];
            maybeMountRuntime({ flyToFirst: true });
        }
    });
}
`
}
