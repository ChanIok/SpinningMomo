export function buildPaneStyleSnippet() {
  return `
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
      '  cursor: default !important;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .leaflet-popup-content {',
      '  max-width: 320px !important;',
      '  padding: 0 !important;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .leaflet-popup-content-wrapper,',
      '.leaflet-popup.' + clusterPopupClassName + ' .leaflet-popup-content-wrapper {',
      '  border-radius: 12px;',
      '  cursor: default !important;',
      '  animation: spinning-momo-popup-enter 160ms cubic-bezier(0.22, 1, 0.36, 1);',
      '  transform-origin: center bottom;',
      '  will-change: opacity, transform;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .leaflet-popup-content-wrapper {',
      '  max-width: 320px !important;',
      '  max-height: 320px !important;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .leaflet-popup-tip,',
      '.leaflet-popup.' + clusterPopupClassName + ' .leaflet-popup-tip {',
      '  animation: spinning-momo-popup-tip-enter 160ms cubic-bezier(0.22, 1, 0.36, 1);',
      '  transform-origin: center top;',
      '  will-change: opacity, transform;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ',',
      '.leaflet-popup.' + clusterPopupClassName + ' {',
      '  cursor: default !important;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .spinning-momo-popup-title,',
      '.leaflet-popup.' + clusterPopupClassName + ' .spinning-momo-popup-title {',
      '  font-size: 13px;',
      '  font-weight: 600;',
      '  line-height: 1.5;',
      '  margin-bottom: 4px;',
      "  color: rgb(123, 93, 74);",
      "  font-family: 'Helvetica Neue', Arial, Helvetica, sans-serif;",
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .spinning-momo-popup-body {',
      '  display: block;',
      '  box-sizing: border-box;',
      '  width: auto;',
      '  max-width: 320px;',
      '  max-height: 320px;',
      '  padding: 0.75rem;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .spinning-momo-popup-thumbnail-block {',
      '  margin-top: 8px;',
      '  max-width: 100%;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .spinning-momo-popup-thumbnail-link {',
      '  display: block;',
      '  max-width: 296px;',
      '  margin: 0;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .spinning-momo-popup-thumbnail-image {',
      '  display: block;',
      '  width: auto;',
      '  height: auto;',
      '  max-width: 296px;',
      '  max-height: calc(320px - 4rem);',
      '  border-radius: 6px;',
      '  background: #f2f2f2;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ' .spinning-momo-popup-thumbnail-fallback {',
      '  font-size: 12px;',
      '  color: #888;',
      '}',
      '@keyframes spinning-momo-popup-enter {',
      '  from {',
      '    opacity: 0;',
      '    transform: translateY(10px) scale(0.96);',
      '  }',
      '  to {',
      '    opacity: 1;',
      '    transform: translateY(0) scale(1);',
      '  }',
      '}',
      '@keyframes spinning-momo-popup-tip-enter {',
      '  from {',
      '    opacity: 0;',
      '    transform: translateY(6px) scaleY(0.9);',
      '  }',
      '  to {',
      '    opacity: 1;',
      '    transform: translateY(0) scaleY(1);',
      '  }',
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
`
}
