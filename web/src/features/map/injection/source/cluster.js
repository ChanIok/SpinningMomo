export function buildClusterSnippet() {
  return `
  const buildClusterHoverGridHtml = (items, gridColumns, cellPx) => {
    const cols = Math.min(3, Math.max(1, gridColumns));
    return (
      '<div data-sm-cluster-grid-root="1" style="display:grid;grid-template-columns:repeat(' +
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
    const previewItems = clusterMarkers.slice(0, previewCount).map((item) => buildPhotoThumbCellHtml(item));

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

    return '<div data-sm-cluster-card="1" style="padding:0.75rem;">' +
      '<div class="spinning-momo-popup-title">' + clusterHeader + '</div>' +
      buildClusterHoverGridHtml(previewItems, gridColumns, cellPx) +
    '</div>';
  };

  const renderClusterMarker = (clusterMarkers) => {
    const lat = clusterMarkers.reduce((sum, item) => sum + Number(item.lat || 0), 0) / clusterMarkers.length;
    const lng = clusterMarkers.reduce((sum, item) => sum + Number(item.lng || 0), 0) / clusterMarkers.length;
    const count = clusterMarkers.length;
    const clusterOwnerId = clusterMarkers
      .map((item) => String(item.assetId ?? item.name ?? (String(item.lat) + ',' + String(item.lng))))
      .join('|');

    const cellPx = 96;

    const applyClusterHoverIncrementalExpand = (hoverCardRoot) => {
      const totalCount = clusterMarkers.length;
      const maxGridCells = 9;
      const previewCount =
        totalCount > maxGridCells ? maxGridCells - 1 : Math.min(totalCount, maxGridCells);
      const remainingCount = Math.max(0, totalCount - previewCount);
      if (remainingCount <= 0) {
        return;
      }

      const grid = hoverCardRoot.querySelector('[data-sm-cluster-grid-root]');
      if (!grid || grid.dataset.smClusterExpanded === 'true') {
        return;
      }

      const expandEl = grid.querySelector('[data-sm-cluster-expand]');
      if (!expandEl) {
        return;
      }

      expandEl.remove();

      const restHtml = clusterMarkers
        .slice(previewCount)
        .map((item) => buildPhotoThumbCellHtml(item))
        .join('');
      if (restHtml) {
        grid.insertAdjacentHTML('beforeend', restHtml);
      }

      grid.style.gridTemplateColumns = 'repeat(3,' + cellPx + 'px)';

      const cardShell = grid.closest('[data-sm-cluster-card]');
      if (cardShell) {
        cardShell.style.display = 'inline-block';
        cardShell.style.maxWidth = 'calc(100vw - 32px)';
      }

      const parentEl = grid.parentElement;
      if (parentEl && parentEl.getAttribute('data-sm-cluster-scroll') !== '1') {
        const sc = document.createElement('div');
        sc.setAttribute('data-sm-cluster-scroll', '1');
        sc.style.maxHeight = 'min(60vh, 420px)';
        sc.style.overflowY = 'auto';
        sc.style.overscrollBehavior = 'contain';
        parentEl.insertBefore(sc, grid);
        sc.appendChild(grid);
      }

      grid.dataset.smClusterExpanded = 'true';

      bindPopupCardClickBridge(hoverCardRoot);
      const scrollRoot = hoverCardRoot.querySelector('[data-sm-cluster-scroll]');
      if (scrollRoot && scrollRoot.dataset.smClusterScrollWheelBound !== 'true') {
        scrollRoot.dataset.smClusterScrollWheelBound = 'true';
        scrollRoot.addEventListener(
          'wheel',
          (event) => {
            event.stopPropagation();
          },
          { passive: true }
        );
      }

      refreshActiveHoverCardPosition();
    };

    const clusterIcon = buildClusterMarkerIcon(count);
    const marker = L.marker([lat, lng], {
      icon: clusterIcon,
      pane: photoPaneName,
      interactive: true,
    }).addTo(runtime.clusterLayer);

    const hoverState = {
      markerHovered: false,
      popupHovered: false,
      openTimer: null,
      closeTimer: null,
    };

    const bindClusterHoverCardInteractions = (rootElement) => {
      if (!rootElement || !rootElement.querySelector) {
        return;
      }

      const expandTarget = rootElement.querySelector('[data-sm-cluster-expand]');
      if (expandTarget && !expandTarget.dataset.smClusterExpandBound) {
        expandTarget.dataset.smClusterExpandBound = 'true';
        expandTarget.addEventListener('click', (event) => {
          event.preventDefault();
          event.stopPropagation();
          hoverState.popupHovered = true;
          const hoverCardRoot = runtime.hoverCardRoot;
          if (!hoverCardRoot) {
            return;
          }
          applyClusterHoverIncrementalExpand(hoverCardRoot);
        });
      }

      const scrollRoot = rootElement.querySelector('[data-sm-cluster-scroll]');
      if (scrollRoot && scrollRoot.dataset.smClusterScrollWheelBound !== 'true') {
        scrollRoot.dataset.smClusterScrollWheelBound = 'true';
        scrollRoot.addEventListener(
          'wheel',
          (event) => {
            event.stopPropagation();
          },
          { passive: true }
        );
      }
    };

    marker.on('mouseover', () => {
      hoverState.markerHovered = true;
      const iconElement = marker.getElement ? marker.getElement() : null;
      if (iconElement) {
        iconElement.style.cursor = 'pointer';
      }
      if (!hoverCardEnabled) return;
      scheduleOpenHoverCard(hoverState, {
        ownerId: clusterOwnerId,
        latLng: [lat, lng],
        contentHtml: buildClusterHoverHtml(clusterMarkers),
        afterOpen: bindClusterHoverCardInteractions,
      });
    });

    marker.on('mouseout', () => {
      hoverState.markerHovered = false;
      if (hoverCardEnabled) {
        if (!hoverState.popupHovered) {
          scheduleClose(hoverState, () => {
            hideHoverCard(clusterOwnerId);
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
