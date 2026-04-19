// 地图 iframe 文档加载后、在劫持 L / 收 postMessage 之前即可运行的页壳逻辑（不依赖 map 实例）。
// 与 runtimeCore 内各 snippet 的区分：后者在 mountOrUpdateMapRuntime 内执行，可访问 runtime / map。

export function buildIframeBootstrapSnippet() {
  return `
  const autoCollapseSidebarOnce = () => {
    if (window.__SPINNING_MOMO_MAP_SIDEBAR_COLLAPSED__) {
      return;
    }

    const toggleSelector =
      '#infinitynikki-map-oversea + div > div > div:nth-child(2) > div:first-child';
    const collapse = () => {
      const toggle = document.querySelector(toggleSelector);
      if (!toggle) return false;
      toggle.click();
      window.__SPINNING_MOMO_MAP_SIDEBAR_COLLAPSED__ = true;
      return true;
    };

    if (collapse()) {
      return;
    }

    let attemptCount = 0;
    const maxAttempts = 40;
    const timer = setInterval(() => {
      attemptCount += 1;
      if (collapse() || attemptCount >= maxAttempts) {
        clearInterval(timer);
      }
    }, 100);
  };

  autoCollapseSidebarOnce();
`
}
