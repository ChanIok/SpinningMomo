export function buildToolbarSnippet() {
  return `
  const markerToggleHostSelector = '#infinitynikki-map-oversea + div > div > div:nth-child(2)';
  const markerToggleButtonId = 'spinning-momo-marker-toggle-button';
  const filterCountCardId = 'spinning-momo-filter-count-card';
  const markerToggleVisibleBg = '#F5DCB1E6';
  const markerToggleHiddenBg = '#4D3E2AE6';

  const getMarkerToggleButtonSize = () => {
    const viewportWidth = Number(window.innerWidth || document.documentElement.clientWidth || 0);
    if (viewportWidth >= 1920) {
      return 48;
    }
    if (viewportWidth >= 1680) {
      return 42;
    }
    return 39;
  };

  const applyMarkerToggleButtonSize = (button) => {
    if (!button) return;
    const buttonSize = getMarkerToggleButtonSize();
    const iconSize = Math.max(24, Math.round(buttonSize * 0.78));
    const padding = Math.max(8, Math.round(buttonSize * 0.2));
    const borderRadius = Math.round(buttonSize / 6);

    button.style.width = String(buttonSize) + 'px';
    button.style.height = String(buttonSize) + 'px';
    button.style.padding = String(padding) + 'px';
    button.style.borderRadius = String(borderRadius) + 'px';

    const icon = button.querySelector('img');
    if (icon) {
      icon.style.width = String(iconSize) + 'px';
      icon.style.height = String(iconSize) + 'px';
    } else {
      button.style.fontSize = String(Math.max(18, Math.round(buttonSize * 0.46))) + 'px';
    }
  };

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

  const syncFilterCountCard = (card) => {
    if (!card) return;

    const currentRuntimeOptions = runtime.runtimeOptions || {};
    const cardVisible = currentRuntimeOptions.filterCountCardVisible !== false;
    const cardLoading = currentRuntimeOptions.filterCountCardLoading === true;
    const cardText = typeof currentRuntimeOptions.filterCountCardText === 'string'
      ? currentRuntimeOptions.filterCountCardText
      : '';
    const bgColor = currentRuntimeOptions.filterCountCardBgColor || '#39311E';
    const textColor = currentRuntimeOptions.filterCountCardTextColor || '#FFFFFF';

    card.style.display = cardVisible ? 'inline-flex' : 'none';
    card.style.backgroundColor = bgColor;
    card.style.color = textColor;
    card.textContent = cardText || (cardLoading ? '正在同步照片坐标…' : '当前筛选下 0 张照片');
  };

  const mountFilterCountCard = () => {
    let card = document.getElementById(filterCountCardId);
    if (!card) {
      card = document.createElement('div');
      card.id = filterCountCardId;
      card.style.display = 'inline-flex';
      card.style.alignItems = 'center';
      card.style.justifyContent = 'center';
      card.style.boxSizing = 'border-box';
      card.style.minHeight = '32px';
      card.style.maxWidth = '220px';
      card.style.padding = '6px 10px';
      card.style.margin = '0 8px 0 0';
      card.style.borderRadius = '8px';
      card.style.whiteSpace = 'nowrap';
      card.style.overflow = 'hidden';
      card.style.textOverflow = 'ellipsis';
      card.style.fontSize = '12px';
      card.style.lineHeight = '1.2';
      card.style.fontWeight = '500';
      card.style.pointerEvents = 'none';
      card.style.position = 'fixed';
      card.style.top = '16px';
      card.style.right = '16px';
      card.style.zIndex = '9999';
      card.style.boxShadow = '0 4px 12px rgba(0, 0, 0, 0.24)';
    }

    if (card.parentElement !== document.body) {
      document.body.appendChild(card);
    }

    syncFilterCountCard(card);
    return true;
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
      button.style.padding = '8px';
      button.style.margin = '0';
      button.style.border = 'none';
      button.style.borderRadius = '7px';
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

    applyMarkerToggleButtonSize(button);
    syncMarkerToggleButton(button);
    return true;
  };

  const ensureMarkerToggleButton = () => {
    mountFilterCountCard();
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

  const ensureMarkerToggleResizeBinding = () => {
    if (runtime.boundMarkerToggleResize) {
      return;
    }

    let resizeTimer = null;
    runtime.boundMarkerToggleResize = () => {
      if (resizeTimer) {
        clearTimeout(resizeTimer);
      }
      resizeTimer = setTimeout(() => {
        resizeTimer = null;
        const button = document.getElementById(markerToggleButtonId);
        if (!button) return;
        applyMarkerToggleButtonSize(button);
      }, 120);
    };

    window.addEventListener('resize', runtime.boundMarkerToggleResize);
  };

  ensureMarkerToggleButton();
  ensureMarkerToggleResizeBinding();
`
}
