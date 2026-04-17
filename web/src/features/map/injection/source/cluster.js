export function buildClusterSnippet() {
  return `
  const buildClusterCellInnerHtml = (item) => {
    if (item && item.thumbnailUrl) {
      return (
        '<img src="' +
        item.thumbnailUrl +
        '" loading="lazy" style="width:100%;height:100%;aspect-ratio:1/1;object-fit:cover;border-radius:6px;background:#1f2937;display:block;" />'
      );
    }

    return (
      '<div style="width:100%;height:100%;aspect-ratio:1/1;border-radius:6px;background:#1f2937;display:flex;align-items:center;justify-content:center;color:#9ca3af;font-size:11px;">无图</div>'
    );
  };

  const buildClusterPreviewCellHtml = (item) => {
    const hasAssetId = item && Number.isFinite(Number(item.assetId));
    const assetIdAttr = hasAssetId ? ' data-sm-open-asset-id="' + String(item.assetId) + '"' : '';
    const hasAssetIndex = item && Number.isFinite(Number(item.assetIndex));
    const assetIndexAttr = hasAssetIndex
      ? ' data-sm-open-asset-index="' + String(item.assetIndex) + '"'
      : '';

    const innerHtml = buildClusterCellInnerHtml(item);
    const cellStyle = hasAssetId ? 'width:100%;height:100%;cursor:pointer;' : 'width:100%;height:100%;';

    return '<div' + assetIdAttr + assetIndexAttr + ' style="' + cellStyle + '">' + innerHtml + '</div>';
  };

  const buildClusterHoverGridHtml = (items, gridColumns, cellPx) => {
    const cols = Math.min(3, Math.max(1, gridColumns));
    return (
      '<div style="display:grid;grid-template-columns:repeat(' +
      cols +
      ',' +
      cellPx +
      'px);grid-auto-rows:' +
      cellPx +
      'px;gap:6px;">' +
      items.join('') +
      '</div>'
    );
  };

  const buildClusterHoverHtml = (clusterMarkers) => {
    const totalCount = clusterMarkers.length;
    const maxGridCells = 9;
    const previewCount = totalCount > maxGridCells ? maxGridCells - 1 : Math.min(totalCount, maxGridCells);
    const remainingCount = Math.max(0, totalCount - previewCount);

    const cellPx = 96;
    const previewItems = clusterMarkers.slice(0, previewCount).map((item) => buildClusterPreviewCellHtml(item));

    if (remainingCount > 0) {
      previewItems.push(
        '<div data-sm-cluster-expand="1" style="width:100%;height:100%;aspect-ratio:1/1;border-radius:6px;background:rgba(17,24,39,0.9);display:flex;align-items:center;justify-content:center;color:#e5e7eb;font-size:13px;font-weight:600;cursor:pointer;">+' +
          remainingCount +
          ' 更多</div>'
      );
    }
    const gridColumns = Math.min(3, Math.max(1, previewItems.length));
    const titleTpl = runtimeOptions.clusterTitleTemplate || '{count} 张照片';
    const clusterHeader = String(titleTpl).replace(/\\{count\\}/g, String(clusterMarkers.length));

    return '<div style="color: #e5e7eb;">' +
      '<div class="spinning-momo-popup-title">' + clusterHeader + '</div>' +
      buildClusterHoverGridHtml(previewItems, gridColumns, cellPx) +
    '</div>';
  };

  const buildClusterHoverExpandedHtml = (clusterMarkers) => {
    const cellPx = 96;
    const titleTpl = runtimeOptions.clusterTitleTemplate || '{count} 张照片';
    const clusterHeader = String(titleTpl).replace(/\\{count\\}/g, String(clusterMarkers.length));
    const allItems = clusterMarkers.map((item) => buildClusterPreviewCellHtml(item));
    const gridColumns = 3;

    return (
      '<div style="display:inline-block; color: #e5e7eb; max-width: calc(100vw - 32px);">' +
      '<div class="spinning-momo-popup-title">' +
      clusterHeader +
      '</div>' +
      '<div data-sm-cluster-scroll="1" style="max-height: min(60vh, 420px); overflow-y: auto; overscroll-behavior: contain;">' +
      buildClusterHoverGridHtml(allItems, gridColumns, cellPx) +
      '</div>' +
      '</div>'
    );
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

      bindPopupCardClickBridge(popupElement);

      const expandTarget = popupElement.querySelector('[data-sm-cluster-expand]');
      if (expandTarget && !expandTarget.dataset.smClusterExpandBound) {
        expandTarget.dataset.smClusterExpandBound = 'true';
        expandTarget.addEventListener('click', (event) => {
          event.preventDefault();
          event.stopPropagation();
          if (!runtime.clusterHoverPopup || !runtime.clusterHoverPopup.setContent) {
            return;
          }
          runtime.clusterHoverPopup.setContent(buildClusterHoverExpandedHtml(clusterMarkers));
          bindClusterPopupHoverBridge();
        });
      }

      const scrollRoot = popupElement.querySelector('[data-sm-cluster-scroll]');
      if (scrollRoot && !scrollRoot.dataset.smClusterScrollWheelBound) {
        scrollRoot.dataset.smClusterScrollWheelBound = 'true';
        scrollRoot.addEventListener(
          'wheel',
          (event) => {
            event.stopPropagation();
          },
          { passive: true }
        );
      }

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
