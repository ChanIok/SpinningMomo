export function buildPaneStyleSnippet() {
  return `
  const photoPaneName = 'spinning-momo-photo-pane';

  const ensureScopedPopupStyles = () => {
    if (document.getElementById('spinning-momo-popup-style')) {
      return;
    }
    const style = document.createElement('style');
    style.id = 'spinning-momo-popup-style';
    style.textContent = [
      '.spinning-momo-hover-card-root {',
      '  position: absolute;',
      '  left: 0;',
      '  top: 0;',
      '  z-index: 1200;',
      '  pointer-events: auto;',
      '}',
      '.spinning-momo-hover-card-root.is-hidden {',
      '  display: none;',
      '}',
      '.spinning-momo-hover-card-shell {',
      '  position: relative;',
      '  display: block;',
      '  width: max-content;',
      '  max-width: 320px;',
      '  border-radius: 12px;',
      '  background: linear-gradient(rgb(240, 222, 208), rgb(245, 236, 227));',
      '  color: rgb(123, 93, 74);',
      '  box-shadow: 0 16px 40px rgba(15, 23, 42, 0.22);',
      '  cursor: default !important;',
      '  will-change: opacity, transform;',
      '}',
      '.spinning-momo-hover-card-root[data-placement="top"] .spinning-momo-hover-card-shell {',
      '  transform-origin: center bottom;',
      '  animation: spinning-momo-hover-card-enter-from-bottom 160ms cubic-bezier(0.22, 1, 0.36, 1);',
      '}',
      '.spinning-momo-hover-card-root[data-placement="bottom"] .spinning-momo-hover-card-shell {',
      '  transform-origin: center top;',
      '  animation: spinning-momo-hover-card-enter-from-top 160ms cubic-bezier(0.22, 1, 0.36, 1);',
      '}',
      '.spinning-momo-hover-card-inner {',
      '  position: relative;',
      '  z-index: 1;',
      '  border-radius: 12px;',
      '  overflow: hidden;',
      '}',
      '.spinning-momo-hover-card-caret {',
      '  position: absolute;',
      '  left: 50%;',
      '  width: 14px;',
      '  height: 14px;',
      '  border-radius: 2px;',
      '  background: linear-gradient(rgb(240, 222, 208), rgb(245, 236, 227));',
      '  transform: translateX(-50%) rotate(45deg);',
      '}',
      '.spinning-momo-hover-card-root[data-placement="top"] .spinning-momo-hover-card-caret {',
      '  bottom: -7px;',
      '}',
      '.spinning-momo-hover-card-root[data-placement="bottom"] .spinning-momo-hover-card-caret {',
      '  top: -7px;',
      '}',
      '.spinning-momo-popup-body {',
      '  display: block;',
      '  box-sizing: border-box;',
      '  width: auto;',
      '  max-width: 320px;',
      '  max-height: 320px;',
      '  padding: 0.75rem;',
      '}',
      '.spinning-momo-popup-title {',
      '  font-size: 13px;',
      '  font-weight: 600;',
      '  line-height: 1.5;',
      '  margin-bottom: 4px;',
      '  color: rgb(123, 93, 74);',
      "  font-family: 'Helvetica Neue', Arial, Helvetica, sans-serif;",
      '}',
      '.spinning-momo-popup-thumbnail-block {',
      '  margin-top: 8px;',
      '  max-width: 100%;',
      '}',
      '.spinning-momo-popup-thumbnail-link {',
      '  display: block;',
      '  max-width: 296px;',
      '  margin: 0;',
      '}',
      '.spinning-momo-popup-thumbnail-image {',
      '  display: block;',
      '  width: auto;',
      '  height: auto;',
      '  max-width: 296px;',
      '  max-height: calc(320px - 4rem);',
      '  border-radius: 6px;',
      '  background: #f2f2f2;',
      '}',
      '.spinning-momo-popup-thumbnail-fallback {',
      '  font-size: 12px;',
      '  color: #888;',
      '}',
      '@keyframes spinning-momo-hover-card-enter-from-bottom {',
      '  from {',
      '    opacity: 0;',
      '    transform: translateY(10px) scale(0.96);',
      '  }',
      '  to {',
      '    opacity: 1;',
      '    transform: translateY(0) scale(1);',
      '  }',
      '}',
      '@keyframes spinning-momo-hover-card-enter-from-top {',
      '  from {',
      '    opacity: 0;',
      '    transform: translateY(-10px) scale(0.96);',
      '  }',
      '  to {',
      '    opacity: 1;',
      '    transform: translateY(0) scale(1);',
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

  ensurePane(photoPaneName, 975);
  ensureScopedPopupStyles();
`
}
