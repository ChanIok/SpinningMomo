export function buildRenderSnippet() {
  return `
  const applyMapBackground = () => {
    const container = map && map.getContainer ? map.getContainer() : null;
    if (!container) return;
    const color = renderOptions.mapBackgroundColor || '#C7BFA7';
    container.style.backgroundColor = String(color);
  };

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

  const renderSingleMarker = (markerData) => {
    const compositeIcon = buildSingleMarkerIcon();
    const markerOwnerId =
      'marker:' + String(markerData.assetId ?? markerData.name ?? (String(markerData.lat) + ',' + String(markerData.lng)));
    const markerOptions = {
      pane: photoPaneName,
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

    if (hoverCardEnabled) {
      marker.on('mouseover', () => {
        hoverState.markerHovered = true;
        scheduleOpenHoverCard(hoverState, {
          ownerId: markerOwnerId,
          latLng: [markerData.lat, markerData.lng],
          contentHtml: buildSinglePhotoHoverHtml(markerData),
        });
      });
      if (closePopupOnMouseOut) {
        marker.on('mouseout', () => {
          hoverState.markerHovered = false;
          invalidatePendingPopupOpen(hoverState);
          const activeContext = runtime.activeHoverCardContext;
          const isPinnedActive =
            activeContext &&
            activeContext.ownerId === markerOwnerId &&
            activeContext.mode === 'pinned';
          if (isPinnedActive) {
            return;
          }
          if (!hoverState.popupHovered) {
            scheduleClose(hoverState, () => hideHoverCard(markerOwnerId));
          }
        });
      }
    }

    marker.on('click', () => {
      pinHoverCard(hoverState, {
        ownerId: markerOwnerId,
        latLng: [markerData.lat, markerData.lng],
        contentHtml: buildSinglePhotoHoverHtml(markerData),
      });
    });
  };

  const render = () => {
    applyMapBackground();
    const activeContext = runtime.activeHoverCardContext;
    const pinnedContext =
      activeContext && activeContext.mode === 'pinned' ? activeContext : null;
    if (!pinnedContext) {
      hideHoverCard();
    }
    runtime.markerLayer.clearLayers();
    runtime.clusterLayer.clearLayers();

    const markersVisible = runtimeOptions.markersVisible !== false;
    if (!markersVisible) {
      if (pinnedContext) {
        hideHoverCard();
      }
      return;
    }

    if (!clusterEnabled || markers.length <= 1 || !map.latLngToLayerPoint) {
      markers.forEach(renderSingleMarker);
    } else {
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
    }

    if (shouldFlyToFirst && markers.length > 0) {
      const firstMarker = markers[0];
      if (firstMarker?.lat !== undefined && firstMarker?.lng !== undefined && map.flyTo) {
        map.flyTo([firstMarker.lat, firstMarker.lng], 6);
      }
    }

    if (pinnedContext) {
      runtime.activeHoverCardContext = {
        ...pinnedContext,
        mode: 'pinned',
      };
      refreshActiveHoverCardPosition();
    }
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
`
}
