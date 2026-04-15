export function buildClusterSnippet() {
  return `
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
    const clusterHeader = String(titleTpl).replace(/\\{count\\}/g, String(clusterMarkers.length));

    return '<div style="display:inline-block; color: #e5e7eb;">' +
      '<div style="font-size: 16px; font-weight: 600; margin-bottom: 8px; color: rgb(123, 93, 74); font-family: &quot;Helvetica Neue&quot;, Arial, Helvetica, sans-serif;">' + clusterHeader + '</div>' +
      '<div style="display:grid;grid-template-columns:repeat(' + gridColumns + ',' + cellPx + 'px);grid-auto-rows:' + cellPx + 'px;gap:6px;">' + previewItems.join('') + '</div>' +
    '</div>';
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
`
}
