import { popupCloseIconDataUri } from './popupCloseIconData.js'

export function buildPopupSnippet() {
  return `
  const pinnedPopupCloseIconDataUri = ${JSON.stringify(popupCloseIconDataUri)};
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

  const ensurePinnedCloseButton = (rootElement, ownerId, visible) => {
    if (!rootElement || !rootElement.querySelector) {
      return;
    }
    const shell = rootElement.querySelector('.spinning-momo-hover-card-shell');
    if (!shell) {
      return;
    }

    let closeButton = shell.querySelector('[data-sm-popup-close="1"]');
    if (!visible) {
      if (closeButton) {
        closeButton.remove();
      }
      return;
    }

    if (!closeButton) {
      closeButton = document.createElement('button');
      closeButton.type = 'button';
      closeButton.className = 'spinning-momo-hover-card-close';
      closeButton.setAttribute('data-sm-popup-close', '1');
      closeButton.setAttribute('aria-label', '关闭');
      shell.insertBefore(closeButton, shell.firstChild || null);
    }

    closeButton.style.backgroundImage = 'url("' + pinnedPopupCloseIconDataUri + '")';
    closeButton.onclick = (event) => {
      event.preventDefault();
      event.stopPropagation();
      hideHoverCard(ownerId || null);
    };
  };

  const clearPopupTimers = (state) => {
    if (!state) {
      return;
    }
    if (state.openTimer) {
      clearTimeout(state.openTimer);
      state.openTimer = null;
    }
    if (state.closeTimer) {
      clearTimeout(state.closeTimer);
      state.closeTimer = null;
    }
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
      if (activeContext.mode === 'pinned') {
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

    const activeContext = runtime.activeHoverCardContext;
    if (activeContext && activeContext.state) {
      clearPopupTimers(activeContext.state);
      activeContext.state.popupHovered = false;
      activeContext.state.markerHovered = false;
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
      const activeContext = runtime.activeHoverCardContext;
      if (activeContext && activeContext.mode === 'pinned') {
        return;
      }
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

    const mode = options.mode === 'pinned' ? 'pinned' : 'hover';
    const showCloseButton = mode === 'pinned';
    const ownerId = options.ownerId || null;
    const sameOwnerActive =
      Boolean(ownerId) &&
      runtime.activeHoverCardOwner === ownerId &&
      runtime.activeHoverCardContext &&
      runtime.hoverCardRoot &&
      !runtime.hoverCardRoot.classList.contains('is-hidden');

    if (!sameOwnerActive) {
      hideHoverCard();
    }

    const placement = options.placement || getHoverCardPlacement(options.latLng);
    if (!sameOwnerActive) {
      root.innerHTML =
        '<div class="spinning-momo-hover-card-shell">' +
        '<div class="spinning-momo-hover-card-inner">' +
        options.contentHtml +
        '</div>' +
        '<div class="spinning-momo-hover-card-caret"></div>' +
        '</div>';
      bindPopupCardClickBridge(root);
      if (typeof options.afterOpen === 'function') {
        options.afterOpen(root);
      }
    }

    if (!positionHoverCardRoot(root, options.latLng, placement)) {
      if (!sameOwnerActive) {
        root.innerHTML = '';
      }
      return null;
    }

    runtime.activeHoverCardOwner = ownerId;
    runtime.activeHoverCardContext = {
      ownerId,
      state,
      latLng: options.latLng,
      mode,
    };
    root.classList.remove('is-hidden');
    ensurePinnedCloseButton(root, ownerId, showCloseButton);

    return root;
  };

  const scheduleOpenHoverCard = (state, cardOptions) => {
    if (!cardOptions || !cardOptions.latLng || !cardOptions.contentHtml) {
      return;
    }
    const activeContext = runtime.activeHoverCardContext;
    if (activeContext && activeContext.mode === 'pinned') {
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
        mode: 'hover',
      });
    });
  };

  const pinHoverCard = (state, cardOptions) => {
    if (!cardOptions || !cardOptions.latLng || !cardOptions.contentHtml) {
      return;
    }
    if (state) {
      state.markerHovered = true;
      state.popupHovered = true;
      clearPopupTimers(state);
    }
    showHoverCard(state, {
      ownerId: cardOptions.ownerId,
      latLng: cardOptions.latLng,
      contentHtml: cardOptions.contentHtml,
      afterOpen: cardOptions.afterOpen,
      placement: cardOptions.placement,
      mode: 'pinned',
    });
  };
`
}
