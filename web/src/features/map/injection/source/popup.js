export function buildPopupSnippet() {
  return `
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
    if (state.openTimer) {
      clearTimeout(state.openTimer);
      state.openTimer = null;
    }
  };

  const escapeHtml = (value) => {
    return String(value || '')
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
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

  const refreshActiveHoverCardPosition = () => {
    const ctx = runtime.activeHoverCardContext;
    const root = runtime.hoverCardRoot;
    if (!ctx || !root || !ctx.latLng) {
      return;
    }
    const nextPlacement = getHoverCardPlacement(ctx.latLng);
    positionHoverCardRoot(root, ctx.latLng, nextPlacement);
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
      latLng: options.latLng,
    };
    root.classList.remove('is-hidden');

    bindPopupCardClickBridge(root);
    if (typeof options.afterOpen === 'function') {
      options.afterOpen(root);
    }

    return root;
  };

  const scheduleOpenHoverCard = (state, cardOptions) => {
    if (!cardOptions || !cardOptions.latLng || !cardOptions.contentHtml) {
      return;
    }
    scheduleOpen(state, () => {
      if (!state.markerHovered && !state.popupHovered) {
        return;
      }
      showHoverCard(state, {
        ownerId: cardOptions.ownerId,
        latLng: cardOptions.latLng,
        contentHtml: cardOptions.contentHtml,
        afterOpen: cardOptions.afterOpen,
        placement: cardOptions.placement,
      });
    });
  };
`
}
