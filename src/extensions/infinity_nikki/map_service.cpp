module;

module Extensions.InfinityNikki.MapService;

import std;
import Core.State;
import Core.WebView;
import Utils.Logger;

namespace Extensions::InfinityNikki::MapService {

auto register_from_settings(Core::State::AppState& app_state) -> void {
#ifdef NDEBUG
  constexpr bool allow_dev_eval = false;
#else
  constexpr bool allow_dev_eval = true;
#endif

  const std::wstring script_part1 =
      LR"SCRIPT(
if (window.location.hostname === 'myl.nuanpaper.com') {
    let innerL = undefined;
    window.__SPINNING_MOMO_ALLOW_DEV_EVAL__ = __ALLOW_DEV_EVAL__;
    window.__SPINNING_MOMO_PENDING_MARKERS__ = [];
    window.__SPINNING_MOMO_RENDER_OPTIONS__ = {};
    window.__SPINNING_MOMO_CLUSTER_OPTIONS__ = {};

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

    const buildCompositePinIcon = (L, overlayInnerHtml) => {
        if (!L || !L.divIcon) {
            return null;
        }
        const options = window.__SPINNING_MOMO_RENDER_OPTIONS__ || {};
        const defaultPinBg = 'https://assets.papegames.com/nikkiweb/infinitynikki/infinitynikki-map/img/58ca045d59db0f9cd8ad.png';
        const pinBgUrl = options.markerPinBackgroundUrl || defaultPinBg;
        const pinSize = 48;
        const pa = options.popupAnchor || [0, -pinSize + 8];
        const bgLayer = '<div style="position:absolute;inset:0;background-image:url(' + JSON.stringify(pinBgUrl) + ');background-size:contain;background-repeat:no-repeat;background-position:center;pointer-events:none;"></div>';
        const html = '<div class="spinning-momo-pin-root" style="width:' + pinSize + 'px;height:' + pinSize + 'px;position:relative;overflow:visible;">' + bgLayer + overlayInnerHtml + '</div>';
        return L.divIcon({
            className: 'spinning-momo-composite-pin',
            html: html,
            iconSize: [pinSize, pinSize],
            iconAnchor: [pinSize / 2, pinSize],
            popupAnchor: pa
        });
    };

    const buildSingleMarkerIcon = (L) => {
        const options = window.__SPINNING_MOMO_RENDER_OPTIONS__ || {};
        const itemUrl = options.markerIconUrl || '';
        const markerIconSize = Array.isArray(options.markerIconSize) ? options.markerIconSize : [24, 24];
        const rawW = Number(markerIconSize[0]);
        const rawH = Number(markerIconSize[1]);
        const itemW = Number.isFinite(rawW) && rawW > 0 ? rawW : 24;
        const itemH = Number.isFinite(rawH) && rawH > 0 ? rawH : itemW;
        const overlay = itemUrl
            ? '<img src="' + itemUrl + '" alt="" style="position:absolute;left:50%;top:40%;transform:translate(calc(-50% - 2px),calc(-50% + 2px));width:' + itemW + 'px;height:' + itemH + 'px;object-fit:contain;pointer-events:none;z-index:1;" />'
            : '';
        return buildCompositePinIcon(L, overlay);
    };

    const ensureInteractivePanes = (map) => {
        if (!map || !map.createPane || !map.getPane) {
            return;
        }

        const markerPaneName = 'spinning-momo-marker-pane';
        const popupPaneName = 'spinning-momo-popup-pane';

        let markerPane = map.getPane(markerPaneName);
        if (!markerPane) {
            markerPane = map.createPane(markerPaneName);
        }
        if (markerPane) {
            markerPane.style.zIndex = '950';
            markerPane.style.pointerEvents = 'auto';
        }

        let popupPane = map.getPane(popupPaneName);
        if (!popupPane) {
            popupPane = map.createPane(popupPaneName);
        }
        if (popupPane) {
            popupPane.style.zIndex = '1000';
            popupPane.style.pointerEvents = 'auto';
        }
    };

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
        ensureInteractivePanes(map);

        layer.clearLayers();
        const customMarkerIcon = buildSingleMarkerIcon(L);
        const renderOptions = window.__SPINNING_MOMO_RENDER_OPTIONS__ || {};
        const openPopupOnHover = renderOptions.openPopupOnHover !== false;
        const closePopupOnMouseOut = renderOptions.closePopupOnMouseOut !== false;

        for (const markerData of markers) {
            if (!markerData) continue;

            const { lat, lng, popupHtml } = markerData;
            if (lat === undefined || lng === undefined) continue;

            const markerOptions = {
                pane: 'spinning-momo-marker-pane',
                interactive: true
            };
            if (customMarkerIcon) {
                markerOptions.icon = customMarkerIcon;
            }

            const marker = L.marker([lat, lng], markerOptions).addTo(layer);
            marker.on('add', () => {
                const iconElement = marker.getElement ? marker.getElement() : null;
                if (iconElement) {
                    iconElement.style.cursor = 'pointer';
                    iconElement.style.pointerEvents = 'auto';
                }
            });
            if (popupHtml) {
                marker.bindPopup(popupHtml, { pane: 'spinning-momo-popup-pane' });
                if (openPopupOnHover) {
                    marker.on('mouseover', () => marker.openPopup());
                }
                if (closePopupOnMouseOut) {
                    marker.on('mouseout', () => marker.closePopup());
                }
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
)SCRIPT";

  const std::wstring script_part2 =
      LR"SCRIPT(
    window.addEventListener('message', (event) => {
        if (!event.data) {
            return;
        }

        if (event.data.action === 'SET_MARKERS') {
            const { markers = [] } = event.data.payload || {};
            renderMarkers(markers);
            return;
        }

        if (event.data.action === 'SET_RENDER_OPTIONS') {
            setRenderOptions(event.data.payload || {});
            renderMarkers(window.__SPINNING_MOMO_PENDING_MARKERS__);
            return;
        }

        if (event.data.action === 'SET_CLUSTER_OPTIONS') {
            setClusterOptions(event.data.payload || {});
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
            renderMarkers([...pendingMarkers, marker], true);
        }
    });
}
)SCRIPT";

  const std::wstring script = script_part1 + script_part2;

  const std::wstring allow_dev_eval_literal = allow_dev_eval ? L"true" : L"false";
  std::wstring script_with_eval_flag = script;
  const std::wstring placeholder = L"__ALLOW_DEV_EVAL__";
  if (const std::size_t pos = script_with_eval_flag.find(placeholder); pos != std::wstring::npos) {
    script_with_eval_flag.replace(pos, placeholder.length(), allow_dev_eval_literal);
  }

  Core::WebView::register_document_created_script(
      app_state, "extensions.infinity_nikki.map_service.bridge", script_with_eval_flag);
  Logger().info("InfinityNikki map WebView bridge script registered");
}

}  // namespace Extensions::InfinityNikki::MapService
