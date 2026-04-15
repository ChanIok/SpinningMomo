export function buildSidebarSnippet() {
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
