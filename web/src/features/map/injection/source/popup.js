export function buildPopupSnippet() {
  return `
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

  const invalidatePendingPopupOpen = (state) => {
    state.pendingOpenRequestId = Number(state.pendingOpenRequestId || 0) + 1;
  };

  const ensurePopupThumbnailCache = () => {
    if (!runtime.popupThumbnailCache) {
      runtime.popupThumbnailCache = new Map();
    }
    return runtime.popupThumbnailCache;
  };

  const preloadPopupThumbnail = (thumbnailUrl) => {
    if (!thumbnailUrl) {
      return Promise.resolve({ ok: false, error: 'missing_url' });
    }

    const cache = ensurePopupThumbnailCache();
    if (cache.has(thumbnailUrl)) {
      return cache.get(thumbnailUrl);
    }

    const pending = new Promise((resolve) => {
      const image = new Image();
      image.decoding = 'async';
      image.onload = () => {
        resolve({
          ok: true,
          width: Number(image.naturalWidth || 0),
          height: Number(image.naturalHeight || 0),
        });
      };
      image.onerror = () => {
        resolve({ ok: false, error: 'load_failed' });
      };
      image.src = thumbnailUrl;
    });

    cache.set(thumbnailUrl, pending);
    return pending;
  };

  const buildPreparedThumbnailBlock = (rootElement, thumbnailUrl, imageMeta) => {
    const body = rootElement.querySelector('.spinning-momo-popup-body');
    if (!body || body.querySelector('.spinning-momo-popup-thumbnail-block')) {
      return;
    }

    const titleElement = body.querySelector('.spinning-momo-popup-title');
    const titleText = titleElement ? titleElement.textContent || '' : '';
    const assetId = body.getAttribute('data-sm-thumbnail-asset-id');
    const assetIndex = body.getAttribute('data-sm-thumbnail-asset-index');

    const thumbnailBlock = document.createElement('div');
    thumbnailBlock.className = 'spinning-momo-popup-thumbnail-block';

    const clickableWrapper = document.createElement('div');
    clickableWrapper.className = 'spinning-momo-popup-thumbnail-link';
    if (assetId !== null && assetId !== undefined) {
      clickableWrapper.style.cursor = 'pointer';
      clickableWrapper.setAttribute('data-sm-open-asset-id', assetId);
      if (assetIndex !== null && assetIndex !== undefined) {
        clickableWrapper.setAttribute('data-sm-open-asset-index', assetIndex);
      }
    }

    if (imageMeta.ok) {
      const image = document.createElement('img');
      image.className = 'spinning-momo-popup-thumbnail-image';
      image.src = thumbnailUrl;
      image.alt = titleText;
      image.decoding = 'async';
      image.loading = 'eager';
      if (Number.isFinite(imageMeta.width) && imageMeta.width > 0) {
        image.width = imageMeta.width;
      }
      if (Number.isFinite(imageMeta.height) && imageMeta.height > 0) {
        image.height = imageMeta.height;
      }
      clickableWrapper.appendChild(image);
    } else {
      const fallback = document.createElement('div');
      fallback.className = 'spinning-momo-popup-thumbnail-fallback';
      fallback.textContent = '缩略图加载失败';
      clickableWrapper.appendChild(fallback);
    }

    thumbnailBlock.appendChild(clickableWrapper);
    body.appendChild(thumbnailBlock);
    body.removeAttribute('data-sm-thumbnail-url');
    body.removeAttribute('data-sm-thumbnail-asset-id');
    body.removeAttribute('data-sm-thumbnail-asset-index');
  };

  const prepareMarkerPopupContent = async (marker) => {
    if (!marker || !marker.getPopup) {
      return;
    }

    const popup = marker.getPopup();
    if (!popup || !popup.getContent || !popup.setContent) {
      return;
    }

    const popupContent = popup.getContent();
    if (typeof popupContent !== 'string' || popupContent.length === 0) {
      return;
    }

    const template = document.createElement('template');
    template.innerHTML = popupContent.trim();
    const rootElement = template.content.firstElementChild;
    if (!rootElement || !rootElement.querySelector) {
      return;
    }

    const body = rootElement.querySelector('.spinning-momo-popup-body');
    if (!body || !body.getAttribute) {
      return;
    }

    const thumbnailUrl = body.getAttribute('data-sm-thumbnail-url');
    if (!thumbnailUrl) {
      return;
    }

    const imageMeta = await preloadPopupThumbnail(thumbnailUrl);
    buildPreparedThumbnailBlock(rootElement, thumbnailUrl, imageMeta);
    popup.setContent(rootElement.outerHTML);
  };

  const openPreparedMarkerPopup = async (state, marker) => {
    if (!marker || !marker.getPopup) {
      return;
    }

    const popup = marker.getPopup();
    if (!popup) {
      return;
    }

    const requestId = Number(state.pendingOpenRequestId || 0) + 1;
    state.pendingOpenRequestId = requestId;

    try {
      await prepareMarkerPopupContent(marker);
    } catch (error) {
      console.error('[SpinningMomo] Failed to prepare marker popup content:', error);
    }

    if (Number(state.pendingOpenRequestId || 0) !== requestId) {
      return;
    }

    if (!state.markerHovered && !state.popupHovered) {
      return;
    }

    marker.openPopup();
  };

  const bindPopupCardClickBridge = (rootElement) => {
    if (!rootElement || !rootElement.querySelectorAll) return;

    const clickableNodes = rootElement.querySelectorAll('[data-sm-open-asset-id]');
    clickableNodes.forEach((node) => {
      const el = node;
      if (!el || !el.getAttribute) return;
      const dataset = el.dataset || {};
      if (dataset.smCardClickBound === 'true') {
        return;
      }

      dataset.smCardClickBound = 'true';

      el.addEventListener('click', (event) => {
        event.preventDefault();
        event.stopPropagation();
        const rawAssetId = el.getAttribute('data-sm-open-asset-id');
        const assetId = Number(rawAssetId);
        if (!Number.isFinite(assetId)) {
          return;
        }

        const rawAssetIndex = el.getAttribute('data-sm-open-asset-index');
        const hasIndex = rawAssetIndex !== null && rawAssetIndex !== undefined;
        const assetIndex = hasIndex ? Number(rawAssetIndex) : undefined;
        if (hasIndex && !Number.isFinite(assetIndex)) {
          return;
        }

        if (window.parent && window.parent !== window) {
          window.parent.postMessage(
            {
              action: 'SPINNING_MOMO_OPEN_GALLERY_ASSET',
              payload: hasIndex ? { assetId, assetIndex } : { assetId },
            },
            '*'
          );
        }
      });
    });
  };

  const bindPopupHoverBridge = (state, marker) => {
    if (!keepPopupVisibleOnHover || !marker || !marker.getPopup) {
      return;
    }

    marker.on('popupopen', () => {
      const popup = marker.getPopup ? marker.getPopup() : null;
      const popupElement = popup && popup.getElement ? popup.getElement() : null;
      if (!popupElement) return;

      bindPopupCardClickBridge(popupElement);

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
`
}
