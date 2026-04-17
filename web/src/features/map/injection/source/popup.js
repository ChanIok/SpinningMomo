export function buildPopupSnippet() {
  return `
  const openPopupOnHover = renderOptions.openPopupOnHover !== false;
  const closePopupOnMouseOut = renderOptions.closePopupOnMouseOut !== false;
  const popupOpenDelayMs = Math.max(0, Number(renderOptions.popupOpenDelayMs ?? 180));
  const popupCloseDelayMs = Math.max(0, Number(renderOptions.popupCloseDelayMs ?? 260));
  const keepPopupVisibleOnHover = renderOptions.keepPopupVisibleOnHover !== false;
  const hoverCardBottomOffsetPx = 16;
  const hoverCardTopOffsetPx = 52;

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

  const ensureHoverCardRoot = () => {
    const container = map && map.getContainer ? map.getContainer() : null;
    if (!container) {
      return null;
    }

    if (runtime.hoverCardRoot && runtime.hoverCardRoot.isConnected) {
      return runtime.hoverCardRoot;
    }

    const root = document.createElement('div');
    root.className = 'spinning-momo-hover-card-root is-hidden';
    root.addEventListener('mouseenter', () => {
      if (!keepPopupVisibleOnHover) {
        return;
      }
      const activeContext = runtime.activeHoverCardContext;
      if (!activeContext) {
        return;
      }
      activeContext.state.popupHovered = true;
      if (activeContext.state.closeTimer) {
        clearTimeout(activeContext.state.closeTimer);
        activeContext.state.closeTimer = null;
      }
    });

    root.addEventListener('mouseleave', () => {
      if (!keepPopupVisibleOnHover) {
        return;
      }
      const activeContext = runtime.activeHoverCardContext;
      if (!activeContext) {
        return;
      }
      activeContext.state.popupHovered = false;
      if (!activeContext.state.markerHovered && closePopupOnMouseOut) {
        scheduleClose(activeContext.state, () => hideHoverCard(activeContext.ownerId));
      }
    });

    container.appendChild(root);
    runtime.hoverCardRoot = root;
    return root;
  };

  const hideHoverCard = (ownerId) => {
    if (ownerId && runtime.activeHoverCardOwner && runtime.activeHoverCardOwner !== ownerId) {
      return;
    }

    const root = runtime.hoverCardRoot;
    if (!root) {
      runtime.activeHoverCardOwner = null;
      runtime.activeHoverCardContext = null;
      return;
    }

    root.innerHTML = '';
    root.classList.add('is-hidden');
    root.removeAttribute('data-placement');
    runtime.activeHoverCardOwner = null;
    runtime.activeHoverCardContext = null;
  };

  const bindHoverCardMapCloseBridge = () => {
    if (runtime.boundHideHoverCardOnMapMove || !map || !map.on) {
      return;
    }

    runtime.boundHideHoverCardOnMapMove = () => {
      hideHoverCard();
    };
    map.on('movestart', runtime.boundHideHoverCardOnMapMove);
    map.on('zoomstart', runtime.boundHideHoverCardOnMapMove);
    map.on('resize', runtime.boundHideHoverCardOnMapMove);
  };

  const getHoverCardPlacement = (latLng) => {
    const container = map && map.getContainer ? map.getContainer() : null;
    const point = map && map.latLngToContainerPoint ? map.latLngToContainerPoint(latLng) : null;
    if (!container || !point) {
      return 'top';
    }

    const containerHeight = Number(container.clientHeight || 0);
    if (!Number.isFinite(containerHeight) || containerHeight <= 0) {
      return 'top';
    }

    return Number(point.y || 0) < containerHeight / 2 ? 'bottom' : 'top';
  };

  const positionHoverCardRoot = (root, latLng, placement) => {
    if (!root || !map || !map.latLngToContainerPoint) {
      return false;
    }

    const point = map.latLngToContainerPoint(latLng);
    if (!point) {
      return false;
    }

    root.style.left = String(Number(point.x || 0)) + 'px';
    if (placement === 'bottom') {
      root.style.top = String(Number(point.y || 0) + hoverCardBottomOffsetPx) + 'px';
      root.style.transform = 'translate(-50%, 0)';
    } else {
      root.style.top = String(Number(point.y || 0) - hoverCardTopOffsetPx) + 'px';
      root.style.transform = 'translate(-50%, -100%)';
    }
    root.dataset.placement = placement;
    return true;
  };

  const prepareHoverCardContent = async (contentHtml) => {
    if (typeof contentHtml !== 'string' || contentHtml.length === 0) {
      return '';
    }

    const template = document.createElement('template');
    template.innerHTML = contentHtml.trim();
    const rootElement = template.content.firstElementChild;
    if (!rootElement || !rootElement.querySelector) {
      return contentHtml;
    }

    const body = rootElement.querySelector('.spinning-momo-popup-body');
    if (!body || !body.getAttribute) {
      return rootElement.outerHTML;
    }

    const thumbnailUrl = body.getAttribute('data-sm-thumbnail-url');
    if (thumbnailUrl) {
      const imageMeta = await preloadPopupThumbnail(thumbnailUrl);
      buildPreparedThumbnailBlock(rootElement, thumbnailUrl, imageMeta);
    }

    return rootElement.outerHTML;
  };

  const showHoverCard = (state, options) => {
    if (!options || !options.latLng) {
      return null;
    }

    bindHoverCardMapCloseBridge();
    const root = ensureHoverCardRoot();
    if (!root) {
      return null;
    }

    const placement = options.placement || getHoverCardPlacement(options.latLng);
    root.innerHTML =
      '<div class="spinning-momo-hover-card-shell">' +
      '<div class="spinning-momo-hover-card-inner">' +
      options.contentHtml +
      '</div>' +
      '<div class="spinning-momo-hover-card-caret"></div>' +
      '</div>';

    if (!positionHoverCardRoot(root, options.latLng, placement)) {
      root.innerHTML = '';
      return null;
    }

    runtime.activeHoverCardOwner = options.ownerId || null;
    runtime.activeHoverCardContext = {
      ownerId: options.ownerId || null,
      state,
    };
    root.classList.remove('is-hidden');

    bindPopupCardClickBridge(root);
    if (typeof options.afterOpen === 'function') {
      options.afterOpen(root);
    }

    return root;
  };

  const openPreparedHoverCard = async (state, options) => {
    if (!options || !options.latLng) {
      return;
    }

    const requestId = Number(state.pendingOpenRequestId || 0) + 1;
    state.pendingOpenRequestId = requestId;

    let preparedContent = '';
    try {
      preparedContent = await prepareHoverCardContent(options.contentHtml);
    } catch (error) {
      console.error('[SpinningMomo] Failed to prepare hover card content:', error);
    }

    if (Number(state.pendingOpenRequestId || 0) !== requestId) {
      return;
    }

    if (!state.markerHovered && !state.popupHovered) {
      return;
    }

    showHoverCard(state, {
      ...options,
      contentHtml: preparedContent,
    });
  };
`
}
