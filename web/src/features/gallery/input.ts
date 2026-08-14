export type GalleryInputType = 'mouse' | 'touch' | 'pen' | 'keyboard'

const RECENT_GALLERY_SCROLL_WINDOW = 250
let lastGalleryScrollAt = Number.NEGATIVE_INFINITY

interface GalleryTouchSourceCapabilities {
  firesTouchEvents?: boolean
}

interface GalleryContextMenuEvent extends MouseEvent {
  // 当前 TypeScript DOM 类型未声明该浏览器属性，但 Chromium/WebView 可能提供它。
  sourceCapabilities?: GalleryTouchSourceCapabilities | null
}

export function normalizeGalleryInputType(pointerType: string): GalleryInputType {
  if (pointerType === 'touch' || pointerType === 'pen') {
    return pointerType
  }

  return 'mouse'
}

export function isGalleryTouchInput(inputType: GalleryInputType): boolean {
  return inputType === 'touch' || inputType === 'pen'
}

// 只有窄屏触摸浏览才把单击解释为“打开暗房”；其余输入仍保留选择/详情语义。
export function shouldOpenAssetOnTap(
  isCompactWindow: boolean,
  inputType: GalleryInputType
): boolean {
  return isCompactWindow && isGalleryTouchInput(inputType)
}

export function markGalleryScroll(): void {
  lastGalleryScrollAt = performance.now()
}

export function isGalleryScrollRecent(): boolean {
  return performance.now() - lastGalleryScrollAt < RECENT_GALLERY_SCROLL_WINDOW
}

export function isGalleryTouchContextMenu(event: MouseEvent, inputType: GalleryInputType): boolean {
  const sourceCapabilities = (event as GalleryContextMenuEvent).sourceCapabilities
  return isGalleryTouchInput(inputType) || sourceCapabilities?.firesTouchEvents === true
}
