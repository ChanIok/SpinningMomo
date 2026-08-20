import { ref } from 'vue'

// 暗房根节点使用 px-[1px]；真实媒体 viewport 已经是内容盒，不应重复扣除这层边距。
export const LIGHTBOX_VIEWPORT_PADDING = 1

interface HeroSource {
  rect: DOMRect
  thumbnailUrl: string
  width: number
  height: number
}

interface ReverseHeroSource {
  fromRect: DOMRect
  toRect: DOMRect
  thumbnailUrl: string
}

let pendingHero: HeroSource | null = null
let pendingReverseHero: ReverseHeroSource | null = null
export const heroAnimating = ref(false)

export function prepareHero(rect: DOMRect, thumbnailUrl: string, width: number, height: number) {
  pendingHero = { rect, thumbnailUrl, width, height }
  heroAnimating.value = true
}

export function consumeHero(): HeroSource | null {
  const h = pendingHero
  pendingHero = null
  return h
}

export function prepareReverseHero(fromRect: DOMRect, toRect: DOMRect, thumbnailUrl: string) {
  pendingReverseHero = { fromRect, toRect, thumbnailUrl }
}

export function computeLightboxHeroRect(
  containerRect: DOMRect | DOMRectReadOnly,
  width: number,
  height: number,
  viewportPadding = LIGHTBOX_VIEWPORT_PADDING
) {
  const contentWidth = Math.max(containerRect.width - viewportPadding * 2, 1)
  const contentHeight = Math.max(containerRect.height - viewportPadding * 2, 1)
  const imageWidth = Math.max(width, 1)
  const imageHeight = Math.max(height, 1)
  const scale = Math.min(contentWidth / imageWidth, contentHeight / imageHeight, 1)
  const targetWidth = imageWidth * scale
  const targetHeight = imageHeight * scale
  const targetX = containerRect.left + (containerRect.width - targetWidth) / 2
  const targetY = containerRect.top + (containerRect.height - targetHeight) / 2

  return new DOMRect(targetX, targetY, targetWidth, targetHeight)
}

export function consumeReverseHero(): ReverseHeroSource | null {
  const h = pendingReverseHero
  pendingReverseHero = null
  return h
}

export function endHeroAnimation() {
  heroAnimating.value = false
}
