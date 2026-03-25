import { ref } from 'vue'

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

export function consumeReverseHero(): ReverseHeroSource | null {
  const h = pendingReverseHero
  pendingReverseHero = null
  return h
}

export function endHeroAnimation() {
  heroAnimating.value = false
}
