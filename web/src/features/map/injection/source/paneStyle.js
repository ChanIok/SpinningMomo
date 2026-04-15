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
      '.leaflet-popup.' + markerPopupClassName + ' .leaflet-popup-content-wrapper,',
      '.leaflet-popup.' + clusterPopupClassName + ' .leaflet-popup-content-wrapper {',
      '  border-radius: 12px;',
      '  cursor: default !important;',
      '}',
      '.leaflet-popup.' + markerPopupClassName + ',',
      '.leaflet-popup.' + clusterPopupClassName + ' {',
      '  cursor: default !important;',
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
