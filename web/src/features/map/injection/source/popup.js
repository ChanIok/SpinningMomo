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
