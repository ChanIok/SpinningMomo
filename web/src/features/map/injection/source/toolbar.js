export function buildToolbarSnippet() {
  return `
  const markerToggleHostSelector = '#infinitynikki-map-oversea + div > div > div:nth-child(2)';
  const markerToggleButtonId = 'spinning-momo-marker-toggle-button';
  const markerToggleVisibleBg = '#F5DCB1E6';
  const markerToggleHiddenBg = '#4D3E2AE6';

  const syncMarkerToggleButton = (button) => {
    if (!button) return;

    const currentRuntimeOptions = runtime.runtimeOptions || {};
    const currentRenderOptions = runtime.renderOptions || {};
    const markersVisible = currentRuntimeOptions.markersVisible !== false;
    const markerIconUrl = currentRenderOptions.markerIconUrl || '';

    button.style.backgroundColor = markersVisible ? markerToggleVisibleBg : markerToggleHiddenBg;
    button.setAttribute('aria-pressed', markersVisible ? 'true' : 'false');
    button.title = markersVisible ? '隐藏照片标点' : '显示照片标点';

    let icon = button.querySelector('img');
    if (markerIconUrl) {
      if (!icon) {
        icon = document.createElement('img');
        icon.alt = '';
        icon.setAttribute('aria-hidden', 'true');
        icon.style.width = '32px';
        icon.style.height = '32px';
        icon.style.objectFit = 'contain';
        icon.style.pointerEvents = 'none';
        button.appendChild(icon);
      }
      icon.src = markerIconUrl;
    } else if (icon && icon.parentNode === button) {
      icon.remove();
      icon = null;
    }

    if (!icon) {
      button.textContent = '•';
      button.style.color = '#FFF7EA';
      button.style.fontSize = '18px';
      button.style.lineHeight = '1';
    } else if (button.textContent) {
      button.textContent = '';
      button.appendChild(icon);
    }
  };

  const mountMarkerToggleButton = () => {
    const host = document.querySelector(markerToggleHostSelector);
    if (!host) return false;

    let button = document.getElementById(markerToggleButtonId);
    if (!button) {
      button = document.createElement('button');
      button.id = markerToggleButtonId;
      button.type = 'button';
      button.style.display = 'flex';
      button.style.alignItems = 'center';
      button.style.justifyContent = 'center';
      button.style.boxSizing = 'border-box';
      button.style.width = '39px';
      button.style.height = '39px';
      button.style.padding = '8.125px';
      button.style.margin = '0';
      button.style.border = 'none';
      button.style.borderRadius = '6.5px';
      button.style.cursor = 'pointer';
      button.style.webkitTapHighlightColor = 'rgba(0, 0, 0, 0)';
      button.style.fontFamily =
        'FZYASHJW_ZHUN, system-ui, -apple-system, "Segoe UI", Roboto, Ubuntu, Cantarell, "Noto Sans", sans-serif, "STHeiti SC", "Microsoft YaHei", "Helvetica Neue", Helvetica, Arial';
      button.style.fontSize = '14px';
      button.style.flexShrink = '0';
      button.addEventListener('click', (event) => {
        event.preventDefault();
        event.stopPropagation();
        const currentRuntimeOptions = runtime.runtimeOptions || {};
        const nextVisible = currentRuntimeOptions.markersVisible === false;

        if (window.parent && window.parent !== window) {
          window.parent.postMessage(
            {
              action: 'SPINNING_MOMO_SET_MARKERS_VISIBLE',
              payload: { markersVisible: nextVisible },
            },
            '*'
          );
        }
      });
    }

    if (button.parentElement !== host) {
      host.appendChild(button);
    }

    syncMarkerToggleButton(button);
    return true;
  };

  const ensureMarkerToggleButton = () => {
    if (mountMarkerToggleButton()) {
      return;
    }

    if (runtime.markerToggleMountTimer) {
      return;
    }

    let attemptCount = 0;
    const maxAttempts = 40;
    runtime.markerToggleMountTimer = setInterval(() => {
      attemptCount += 1;
      if (mountMarkerToggleButton() || attemptCount >= maxAttempts) {
        clearInterval(runtime.markerToggleMountTimer);
        runtime.markerToggleMountTimer = null;
      }
    }, 100);
  };

  ensureMarkerToggleButton();
`
}
