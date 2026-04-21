/**
 * 地图 iframe 与宿主的运行时挂钩：flush 由 MapIframeHost 注册；
 * iframe 发来 session ready / world 时先执行 `afterWorldFromIframe`（重算 markers），再 flush。
 */

let flushImpl: (() => void) | null = null
let afterWorldFromIframeImpl: (() => void) | null = null

export function registerMapIframeFlush(fn: () => void) {
  flushImpl = fn
}

export function registerMapAfterIframeWorld(fn: () => void) {
  afterWorldFromIframeImpl = fn
}

export function flushMapRuntimeToIframe() {
  flushImpl?.()
}

/** iframe 报告地图已就绪或 URL 中 world 变化：先按新世界过滤标点，再推一帧 */
export function applyIframeSessionReadyThenFlush() {
  afterWorldFromIframeImpl?.()
  flushImpl?.()
}
