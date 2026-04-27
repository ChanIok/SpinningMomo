export function buildWorldIdBridgeSnippet() {
  return `
  const postWorldIdChanged = (worldId) => {
    if (worldId === runtime.lastWorldIdValue) {
      return;
    }
    runtime.lastWorldIdValue = worldId;
    if (window.parent && window.parent !== window) {
      window.parent.postMessage(
        {
          action: 'SPINNING_MOMO_MAP_SESSION_READY',
          payload: worldId ? { worldId } : {},
        },
        '*'
      );
    }
  };

  const syncCurrentWorldIdFromMapState = () => {
    postWorldIdChanged(readOfficialCurrentWorldId());
  };

  if (!runtime.boundWorldIdBridge) {
    runtime.boundWorldIdBridge = true;
    runtime.boundWorldIdPopstate = () => syncCurrentWorldIdFromMapState();
    runtime.boundWorldIdHashchange = () => syncCurrentWorldIdFromMapState();
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
        syncCurrentWorldIdFromMapState();
        return result;
      };
    };

    patchHistoryMethod('pushState');
    patchHistoryMethod('replaceState');
  }

  syncCurrentWorldIdFromMapState();
`
}
