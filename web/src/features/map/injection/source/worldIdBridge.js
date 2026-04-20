export function buildWorldIdBridgeSnippet() {
  return `
  const readCurrentWorldId = () => {
    try {
      const searchParams = new URLSearchParams(window.location.search || '');
      const rawWorldId = String(searchParams.get('worldId') || '').trim();
      return rawWorldId || undefined;
    } catch {
      return undefined;
    }
  };

  const postWorldIdChanged = (worldId) => {
    if (worldId === runtime.lastWorldIdValue) {
      return;
    }
    runtime.lastWorldIdValue = worldId;
    if (window.parent && window.parent !== window) {
      window.parent.postMessage(
        {
          action: 'SPINNING_MOMO_MAP_WORLD_CHANGED',
          payload: worldId ? { worldId } : {},
        },
        '*'
      );
    }
  };

  const syncWorldIdFromLocation = () => {
    postWorldIdChanged(readCurrentWorldId());
  };

  if (!runtime.boundWorldIdBridge) {
    runtime.boundWorldIdBridge = true;
    runtime.boundWorldIdPopstate = () => syncWorldIdFromLocation();
    runtime.boundWorldIdHashchange = () => syncWorldIdFromLocation();
    window.addEventListener('popstate', runtime.boundWorldIdPopstate);
    window.addEventListener('hashchange', runtime.boundWorldIdHashchange);

    const patchHistoryMethod = (name) => {
      const originalMethod = window.history && window.history[name];
      if (typeof originalMethod !== 'function') {
        return;
      }
      if (runtime['patchedHistory_' + name]) {
        return;
      }
      runtime['patchedHistory_' + name] = true;
      window.history[name] = function(...args) {
        const result = originalMethod.apply(this, args);
        syncWorldIdFromLocation();
        return result;
      };
    };

    patchHistoryMethod('pushState');
    patchHistoryMethod('replaceState');
  }

  syncWorldIdFromLocation();
`
}
