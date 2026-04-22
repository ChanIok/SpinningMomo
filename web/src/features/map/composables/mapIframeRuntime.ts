/** 地图 iframe 与宿主的运行时挂钩：flush 由 MapIframeHost 注册。 */

let flushImpl: (() => void) | null = null

export function registerMapIframeFlush(fn: () => void) {
  flushImpl = fn
}

export function flushMapRuntimeToIframe() {
  flushImpl?.()
}
