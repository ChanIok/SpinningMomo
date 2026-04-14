module;

module Extensions.InfinityNikki.MapService;

import std;
import Core.State;
import Core.WebView;
import Utils.Logger;

namespace Extensions::InfinityNikki::MapService {

auto register_from_settings(Core::State::AppState& app_state) -> void {
  const std::wstring script =
      LR"SCRIPT(
if (window.location.hostname === 'myl.nuanpaper.com') {
    let innerL = undefined;
    window.__SPINNING_MOMO_PENDING_MARKERS__ = [];

    const renderMarkers = (markers, shouldFlyToFirst = false) => {
        window.__SPINNING_MOMO_PENDING_MARKERS__ = Array.isArray(markers) ? markers : [];

        const map = window.__SPINNING_MOMO_MAP__;
        const L = window.L;
        if (!map || !L) {
            return;
        }

        let layer = window.__SPINNING_MOMO_MARKER_LAYER__;
        if (!layer) {
            layer = L.layerGroup().addTo(map);
            window.__SPINNING_MOMO_MARKER_LAYER__ = layer;
        }

        layer.clearLayers();

        for (const markerData of markers) {
            if (!markerData) continue;

            const { lat, lng, popupHtml } = markerData;
            if (lat === undefined || lng === undefined) continue;

            const marker = L.marker([lat, lng]).addTo(layer);
            if (popupHtml) {
                marker.bindPopup(popupHtml);
            }
        }

        if (shouldFlyToFirst && markers.length > 0) {
            const firstMarker = markers[0];
            if (firstMarker?.lat !== undefined && firstMarker?.lng !== undefined) {
                map.flyTo([firstMarker.lat, firstMarker.lng], 6);
            }
        }
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
                    if (window.__SPINNING_MOMO_PENDING_MARKERS__.length > 0) {
                        renderMarkers(window.__SPINNING_MOMO_PENDING_MARKERS__);
                    }
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
            renderMarkers(markers);
            return;
        }

        if (event.data.action === 'ADD_MARKER') {
            const marker = event.data.payload;
            if (!marker) {
                return;
            }

            const pendingMarkers = window.__SPINNING_MOMO_PENDING_MARKERS__;
            renderMarkers([...pendingMarkers, marker], true);
        }
    });
}
)SCRIPT";

  Core::WebView::register_document_created_script(
      app_state, "extensions.infinity_nikki.map_service.bridge", script);
  Logger().info("InfinityNikki map WebView bridge script registered");
}

}  // namespace Extensions::InfinityNikki::MapService
