import { nextTick } from 'vue'

type MinimalViewTransition = {
  finished: Promise<void>
}

type DocumentWithViewTransition = Document & {
  startViewTransition?: (updateCallback: () => void | Promise<void>) => MinimalViewTransition
}

let activeLayoutTransitions = 0

async function waitForVisibleCardImages(doc: Document): Promise<void> {
  const scene = doc.querySelector<HTMLElement>('.gallery-layout-scene')
  if (!scene) {
    return
  }

  const sceneRect = scene.getBoundingClientRect()
  if (sceneRect.width <= 0 || sceneRect.height <= 0) {
    return
  }

  // 动态通过 DOM 视口矩形相交判定当前可见卡片或列表行，零硬编码数量或固定行数
  const itemEls = scene.querySelectorAll<HTMLElement>('[data-asset-card], [data-asset-list-row]')
  const visibleImages: HTMLImageElement[] = []

  for (const item of itemEls) {
    const rect = item.getBoundingClientRect()
    const inViewport =
      rect.bottom > sceneRect.top &&
      rect.top < sceneRect.bottom &&
      rect.right > sceneRect.left &&
      rect.left < sceneRect.right

    if (inViewport) {
      const img = item.querySelector<HTMLImageElement>('img')
      if (img && img.src) {
        visibleImages.push(img)
      }
    }
  }

  if (visibleImages.length === 0) {
    return
  }

  const decodePromises = visibleImages.map((img) => img.decode().catch(() => undefined))
  await Promise.race([
    Promise.allSettled(decodePromises),
    new Promise((resolve) => setTimeout(resolve, 250)),
  ])

  // 解码完成后让出微任务，确保 Vue 响应式状态（隐藏占位遮罩）在快照捕获前生效
  await nextTick()
}

/**
 * 局部触发画廊内容视口的 View Transition 过渡动画。
 * 在回调执行期间设置 data-transition="gallery-layout"，
 * 使路由层（.route-scene）与根节点保持静止，仅对画廊内容区做平滑淡入淡出。
 */
export async function runWithLayoutTransition(update: () => Promise<void> | void): Promise<void> {
  const doc = typeof document !== 'undefined' ? (document as DocumentWithViewTransition) : undefined

  if (!doc?.startViewTransition) {
    await update()
    return
  }

  activeLayoutTransitions += 1
  doc.documentElement.dataset.transition = 'gallery-layout'

  try {
    const transition = doc.startViewTransition(async () => {
      await update()
      await nextTick()
      await waitForVisibleCardImages(doc)
    })
    await transition.finished.catch(() => undefined)
  } finally {
    activeLayoutTransitions -= 1
    if (activeLayoutTransitions <= 0) {
      activeLayoutTransitions = 0
      delete doc.documentElement.dataset.transition
    }
  }
}
