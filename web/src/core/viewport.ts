/** 应用窗口进入紧凑布局的宽度边界：< 该值为紧凑视图，>= 该值为宽屏。 */
export const COMPACT_WINDOW_BREAKPOINT = 640

/** 宽屏视口查询，供 JS 侧统一判定布局使用。 */
export const WIDE_VIEWPORT_QUERY = `(min-width: ${COMPACT_WINDOW_BREAKPOINT}px)`

/** 紧凑视图判定，与 WIDE_VIEWPORT_QUERY 严格互补，避免 max-width 与 min-width 之间的宽度空隙。 */
export const isCompactViewport = (): boolean =>
  typeof window !== 'undefined' && !window.matchMedia(WIDE_VIEWPORT_QUERY).matches
