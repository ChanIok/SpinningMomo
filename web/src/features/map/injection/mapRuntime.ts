import type { MapMarker, MapRenderOptions, MapRuntimeOptions } from '@/features/map/store'

type RuntimePayload = {
  markers: MapMarker[]
  renderOptions: MapRenderOptions
  runtimeOptions: MapRuntimeOptions
}

export function buildMapRuntimeScript(payload: RuntimePayload): string {
  const serializedPayload = JSON.stringify(payload).replace(/</g, '\\u003c')

  return `
(() => {
  if (window.location.hostname !== 'myl.nuanpaper.com') return;
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

  const markerPaneName = 'spinning-momo-marker-pane';
  const popupPaneName = 'spinning-momo-popup-pane';
  const clusterPaneName = 'spinning-momo-cluster-pane';
  const clusterPopupPaneName = 'spinning-momo-cluster-popup-pane';
  const markerPopupClassName = 'spinning-momo-marker-popup';
  const clusterPopupClassName = 'spinning-momo-cluster-hover-popup';

  const ensureScopedPopupStyles = () => {
    if (document.getElementById('spinning-momo-popup-style')) {
      return;
    }
    const style = document.createElement('style');
    style.id = 'spinning-momo-popup-style';
    style.textContent = [
      '.leaflet-popup.' + markerPopupClassName + ' .leaflet-popup-content,',
      '.leaflet-popup.' + clusterPopupClassName + ' .leaflet-popup-content {',
      '  width: auto !important;',
      '  max-width: 320px !important;',
      '  margin: 0 !important;',
      '  padding: 0.75rem !important;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .leaflet-popup-content-wrapper,',
      '.leaflet-popup.' + clusterPopupClassName + ' .leaflet-popup-content-wrapper {',
      '  border-radius: 12px;',
      '}',
    ].join('\\n');
    document.head.appendChild(style);
  };

  const ensurePane = (paneName, zIndex) => {
    let pane = map.getPane ? map.getPane(paneName) : null;
    if (!pane && map.createPane) pane = map.createPane(paneName);
    if (pane) {
      pane.style.zIndex = String(zIndex);
      pane.style.pointerEvents = 'auto';
    }
    return pane;
  };

  ensurePane(markerPaneName, 950);
  ensurePane(popupPaneName, 1000);
  ensurePane(clusterPaneName, 980);
  ensurePane(clusterPopupPaneName, 1100);
  ensureScopedPopupStyles();

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
      autoPan: false,
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

  const defaultPinBg =
    'https://assets.papegames.com/nikkiweb/infinitynikki/infinitynikki-map/img/58ca045d59db0f9cd8ad.png';
  const pinBgUrl = renderOptions.markerPinBackgroundUrl || defaultPinBg;
  const pinSize = 36;
  const markerIconSize = renderOptions.markerIconSize || [24, 24];
  const rawW = Number(markerIconSize[0]);
  const rawH = Number(markerIconSize[1]);
  const itemW = Number.isFinite(rawW) && rawW > 0 ? rawW : 24;
  const itemH = Number.isFinite(rawH) && rawH > 0 ? rawH : itemW;

  const buildCompositePinIcon = (overlayInnerHtml) => {
    if (!L.divIcon) return null;
    const pa = renderOptions.popupAnchor || [0, -pinSize + 8];
    const html =
      '<div class="spinning-momo-pin-root" style="width:' +
      pinSize +
      'px;height:' +
      pinSize +
      'px;position:relative;overflow:visible;">' +
      '<div style="position:absolute;inset:0;background-image:url(\\'' +
      pinBgUrl +
      '\\');background-size:contain;background-repeat:no-repeat;background-position:center;pointer-events:none;"></div>' +
      overlayInnerHtml +
      '</div>';
    return L.divIcon({
      className: 'spinning-momo-composite-pin',
      html: html,
      iconSize: [pinSize, pinSize],
      iconAnchor: [pinSize / 2, pinSize],
      popupAnchor: pa,
    });
  };

  const buildSingleMarkerIcon = () => {
    const itemUrl = renderOptions.markerIconUrl || '';
    const overlay = itemUrl
      ? '<img src="' +
        itemUrl +
        '" alt="" style="position:absolute;left:50%;top:40%;transform:translate(calc(-50% - 0.25px),calc(-50% + 1.8px));width:' +
        itemW +
        'px;height:' +
        itemH +
        'px;object-fit:contain;pointer-events:none;z-index:1;" />'
      : '';
    return buildCompositePinIcon(overlay);
  };

  const buildClusterMarkerIcon = (count) => {
    const overlay =
      '<div style="position:absolute;left:0;top:0;right:0;bottom:0;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:700;font-size:15px;line-height:1;text-shadow:0 1px 3px rgba(0,0,0,0.85);pointer-events:none;transform:translateY(-3px);">' +
      count +
      '</div>';
    return buildCompositePinIcon(overlay);
  };
  const openPopupOnHover = renderOptions.openPopupOnHover !== false;
  const closePopupOnMouseOut = renderOptions.closePopupOnMouseOut !== false;
  const popupOpenDelayMs = Math.max(0, Number(renderOptions.popupOpenDelayMs ?? 180));
  const popupCloseDelayMs = Math.max(0, Number(renderOptions.popupCloseDelayMs ?? 260));
  const keepPopupVisibleOnHover = renderOptions.keepPopupVisibleOnHover !== false;

  const scheduleOpen = (state, callback) => {
    if (state.closeTimer) {
      clearTimeout(state.closeTimer);
      state.closeTimer = null;
    }
    if (state.openTimer) {
      clearTimeout(state.openTimer);
    }
    state.openTimer = setTimeout(() => {
      state.openTimer = null;
      callback();
    }, popupOpenDelayMs);
  };

  const scheduleClose = (state, callback) => {
    if (state.openTimer) {
      clearTimeout(state.openTimer);
      state.openTimer = null;
    }
    if (state.closeTimer) {
      clearTimeout(state.closeTimer);
    }
    state.closeTimer = setTimeout(() => {
      state.closeTimer = null;
      callback();
    }, popupCloseDelayMs);
  };

  const bindPopupHoverBridge = (state, marker) => {
    if (!keepPopupVisibleOnHover || !marker || !marker.getPopup) {
      return;
    }

    marker.on('popupopen', () => {
      const popup = marker.getPopup ? marker.getPopup() : null;
      const popupElement = popup && popup.getElement ? popup.getElement() : null;
      if (!popupElement) return;

      popupElement.addEventListener('mouseenter', () => {
        state.popupHovered = true;
        if (state.closeTimer) {
          clearTimeout(state.closeTimer);
          state.closeTimer = null;
        }
      });

      popupElement.addEventListener('mouseleave', () => {
        state.popupHovered = false;
        if (!state.markerHovered && closePopupOnMouseOut) {
          scheduleClose(state, () => marker.closePopup());
        }
      });
    });
  };

  const buildClusterHoverHtml = (clusterMarkers) => {
    const totalCount = clusterMarkers.length;
    const maxGridCells = 9;
    const previewCount = totalCount > maxGridCells ? maxGridCells - 1 : Math.min(totalCount, maxGridCells);
    const remainingCount = Math.max(0, totalCount - previewCount);

    const cellPx = 96;
    const previewItems = clusterMarkers.slice(0, previewCount).map((item) => {
      if (item.thumbnailUrl) {
        return '<img src="' + item.thumbnailUrl + '" loading="lazy" style="width:100%;height:100%;aspect-ratio:1/1;object-fit:cover;border-radius:6px;background:#1f2937;display:block;" />';
      }
      return '<div style="width:100%;height:100%;aspect-ratio:1/1;border-radius:6px;background:#1f2937;display:flex;align-items:center;justify-content:center;color:#9ca3af;font-size:11px;">无图</div>';
    });

    if (remainingCount > 0) {
      previewItems.push(
        '<div style="width:100%;height:100%;aspect-ratio:1/1;border-radius:6px;background:rgba(17,24,39,0.9);display:flex;align-items:center;justify-content:center;color:#e5e7eb;font-size:13px;font-weight:600;">+' +
          remainingCount +
          ' 更多</div>'
      );
    }
    const gridColumns = Math.min(3, Math.max(1, previewItems.length));
    const titleTpl = runtimeOptions.clusterTitleTemplate || '{count} 张照片';
    const clusterHeader = String(titleTpl).replace(/\{count\}/g, String(clusterMarkers.length));

    return '<div style="display:inline-block; color: #e5e7eb;">' +
      '<div style="font-size: 16px; font-weight: 600; margin-bottom: 8px; color: rgb(123, 93, 74); font-family: &quot;Helvetica Neue&quot;, Arial, Helvetica, sans-serif;">' + clusterHeader + '</div>' +
      '<div style="display:grid;grid-template-columns:repeat(' + gridColumns + ',' + cellPx + 'px);grid-auto-rows:' + cellPx + 'px;gap:6px;">' + previewItems.join('') + '</div>' +
    '</div>';
  };

  const renderSingleMarker = (markerData) => {
    const compositeIcon = buildSingleMarkerIcon();
    const markerOptions = {
      pane: markerPaneName,
      interactive: true,
    };
    if (compositeIcon) markerOptions.icon = compositeIcon;

    const marker = L.marker([markerData.lat, markerData.lng], markerOptions).addTo(runtime.markerLayer);
    const hoverState = {
      markerHovered: false,
      popupHovered: false,
      openTimer: null,
      closeTimer: null,
    };
    marker.on('add', () => {
      const iconElement = marker.getElement ? marker.getElement() : null;
      if (iconElement) {
        iconElement.style.cursor = 'pointer';
        iconElement.style.pointerEvents = 'auto';
      }
    });

    if (markerData.popupHtml) {
      marker.bindPopup(markerData.popupHtml, {
        pane: popupPaneName,
        className: markerPopupClassName,
      });
      bindPopupHoverBridge(hoverState, marker);
      if (openPopupOnHover) {
        marker.on('mouseover', () => {
          hoverState.markerHovered = true;
          scheduleOpen(hoverState, () => marker.openPopup());
        });
      }
      if (closePopupOnMouseOut) {
        marker.on('mouseout', () => {
          hoverState.markerHovered = false;
          if (!hoverState.popupHovered) {
            scheduleClose(hoverState, () => marker.closePopup());
          }
        });
      }
    }
  };

  const renderClusterMarker = (clusterMarkers) => {
    const lat = clusterMarkers.reduce((sum, item) => sum + Number(item.lat || 0), 0) / clusterMarkers.length;
    const lng = clusterMarkers.reduce((sum, item) => sum + Number(item.lng || 0), 0) / clusterMarkers.length;
    const count = clusterMarkers.length;
    const clusterOwnerId = clusterMarkers
      .map((item) => String(item.assetId ?? item.name ?? (String(item.lat) + ',' + String(item.lng))))
      .join('|');

    const clusterIcon = buildClusterMarkerIcon(count);

    const marker = L.marker([lat, lng], {
      icon: clusterIcon,
      pane: clusterPaneName,
      interactive: true,
    }).addTo(runtime.clusterLayer);

    const hoverState = {
      markerHovered: false,
      popupHovered: false,
      openTimer: null,
      closeTimer: null,
    };

    const bindClusterPopupHoverBridge = () => {
      if (!keepPopupVisibleOnHover || !runtime.clusterHoverPopup || !runtime.clusterHoverPopup.getElement) return;
      const popupElement = runtime.clusterHoverPopup.getElement();
      if (!popupElement) return;

      popupElement.onmouseenter = () => {
        hoverState.popupHovered = true;
        if (hoverState.closeTimer) {
          clearTimeout(hoverState.closeTimer);
          hoverState.closeTimer = null;
        }
      };
      popupElement.onmouseleave = () => {
        hoverState.popupHovered = false;
        if (!hoverState.markerHovered) {
          scheduleClose(hoverState, () => map.closePopup(runtime.clusterHoverPopup));
        }
      };
    };

    marker.on('mouseover', () => {
      hoverState.markerHovered = true;
      const iconElement = marker.getElement ? marker.getElement() : null;
      if (iconElement) {
        iconElement.style.cursor = 'pointer';
      }
      if (!hoverCardEnabled) return;
      scheduleOpen(hoverState, () => {
        runtime.activeClusterPopupOwner = clusterOwnerId;
        runtime.clusterHoverPopup
          .setLatLng([lat, lng])
          .setContent(buildClusterHoverHtml(clusterMarkers))
          .openOn(map);
        bindClusterPopupHoverBridge();
      });
    });

    marker.on('mouseout', () => {
      hoverState.markerHovered = false;
      if (hoverCardEnabled) {
        if (!hoverState.popupHovered) {
          scheduleClose(hoverState, () => {
            if (runtime.activeClusterPopupOwner !== clusterOwnerId) {
              return;
            }
            runtime.activeClusterPopupOwner = null;
            map.closePopup(runtime.clusterHoverPopup);
          });
        }
      }
    });

    marker.on('click', () => {
      const nextZoom = Math.min((map.getZoom ? map.getZoom() : 6) + 2, map.getMaxZoom ? map.getMaxZoom() : 18);
      if (map.flyTo) {
        map.flyTo([lat, lng], nextZoom);
      }
    });
  };

  const render = () => {
    runtime.markerLayer.clearLayers();
    runtime.clusterLayer.clearLayers();

    if (!clusterEnabled || markers.length <= 1 || !map.latLngToLayerPoint) {
      markers.forEach(renderSingleMarker);
      return;
    }

    const grouped = new Map();
    for (const item of markers) {
      if (item.lat === undefined || item.lng === undefined) continue;
      const point = map.latLngToLayerPoint([item.lat, item.lng]);
      const gridX = Math.round(point.x / clusterRadius);
      const gridY = Math.round(point.y / clusterRadius);
      const key = gridX + ':' + gridY;
      if (!grouped.has(key)) grouped.set(key, []);
      grouped.get(key).push(item);
    }

    grouped.forEach((clusterMarkers) => {
      if (clusterMarkers.length <= 1) {
        renderSingleMarker(clusterMarkers[0]);
        return;
      }
      renderClusterMarker(clusterMarkers);
    });
  };

  runtime.render = render;
  render();

  if (!runtime.boundRecluster) {
    runtime.boundRecluster = () => {
      if (runtime.render) runtime.render();
    };
    if (map.on) {
      map.on('zoomend', runtime.boundRecluster);
      map.on('moveend', runtime.boundRecluster);
    }
  }
})();
`
}
