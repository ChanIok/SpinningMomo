import { useGalleryStore } from '../store'

const GALLERY_ASSET_DRAG_MIME = 'application/x-spinningmomo-gallery-asset-ids'
const DRAG_PREVIEW_MAX_EDGE = 128
const DRAG_PREVIEW_RADIUS = 4
// drag image 必须挂在真实 DOM 上；这里保存当前临时节点，便于 dragend 清理。
let activeDragPreview: HTMLElement | null = null

function parseAssetIds(raw: string): number[] {
  if (!raw) {
    return []
  }

  try {
    const parsed = JSON.parse(raw) as unknown
    if (!Array.isArray(parsed)) {
      return []
    }
    return parsed.map((id) => Number(id)).filter((id) => Number.isInteger(id) && id > 0)
  } catch {
    return []
  }
}

function cleanupDragPreview() {
  if (!activeDragPreview) {
    return
  }
  activeDragPreview.remove()
  activeDragPreview = null
}

function createDragPreview(sourceEl: HTMLElement, selectedCount: number): HTMLElement {
  // 列表视图用“纯数字圆点”，避免覆盖侧边栏文本。
  const isListRowDrag = !!sourceEl.closest('[data-asset-list-row]')
  if (isListRowDrag) {
    const badgeOnly = document.createElement('div')
    badgeOnly.style.position = 'fixed'
    badgeOnly.style.left = '-10000px'
    badgeOnly.style.top = '-10000px'
    badgeOnly.style.width = '28px'
    badgeOnly.style.height = '28px'
    badgeOnly.style.borderRadius = '999px'
    badgeOnly.style.background = 'rgba(0, 0, 0, 0.78)'
    badgeOnly.style.color = '#fff'
    badgeOnly.style.fontSize = '12px'
    badgeOnly.style.fontWeight = '700'
    badgeOnly.style.display = 'flex'
    badgeOnly.style.alignItems = 'center'
    badgeOnly.style.justifyContent = 'center'
    badgeOnly.style.pointerEvents = 'none'
    badgeOnly.style.boxShadow = '0 8px 18px rgba(0, 0, 0, 0.28)'
    badgeOnly.textContent = String(Math.max(1, selectedCount))
    document.body.appendChild(badgeOnly)
    activeDragPreview = badgeOnly
    return badgeOnly
  }

  const previewShell = document.createElement('div')
  const rect = sourceEl.getBoundingClientRect()
  const width = Math.max(1, rect.width)
  const height = Math.max(1, rect.height)
  const scale = Math.min(1, DRAG_PREVIEW_MAX_EDGE / Math.max(width, height))
  const previewWidth = Math.round(width * scale)
  const previewHeight = Math.round(height * scale)

  previewShell.style.position = 'fixed'
  previewShell.style.left = '-10000px'
  previewShell.style.top = '-10000px'
  previewShell.style.width = `${previewWidth}px`
  previewShell.style.height = `${previewHeight}px`
  previewShell.style.pointerEvents = 'none'
  previewShell.style.margin = '0'
  previewShell.style.boxSizing = 'border-box'
  previewShell.style.overflow = 'hidden'
  previewShell.style.borderRadius = `${DRAG_PREVIEW_RADIUS}px`
  previewShell.style.background = 'rgba(15, 23, 42, 0.35)'
  previewShell.style.boxShadow = '0 10px 24px rgba(0, 0, 0, 0.24)'

  // 优先只取图片层，避免把卡片 hover/ring 等复杂样式带进拖拽预览。
  const media = sourceEl.querySelector('img')?.cloneNode(true) as HTMLImageElement | undefined
  if (media) {
    media.style.width = '100%'
    media.style.height = '100%'
    media.style.objectFit = 'cover'
    media.style.pointerEvents = 'none'
    previewShell.appendChild(media)
  } else {
    const fallback = sourceEl.cloneNode(true) as HTMLElement
    fallback
      .querySelectorAll('[data-selection-indicator], [data-selection-mask]')
      .forEach((node) => {
        node.remove()
      })
    fallback.style.width = '100%'
    fallback.style.height = '100%'
    fallback.style.margin = '0'
    fallback.style.borderRadius = `${DRAG_PREVIEW_RADIUS}px`
    fallback.style.overflow = 'hidden'
    fallback.style.pointerEvents = 'none'
    previewShell.appendChild(fallback)
  }

  if (selectedCount > 1) {
    // 多选时叠加数量角标，提示这是批量拖拽。
    const badge = document.createElement('div')
    badge.textContent = String(selectedCount)
    badge.style.position = 'absolute'
    badge.style.right = '6px'
    badge.style.top = '6px'
    badge.style.minWidth = '20px'
    badge.style.height = '20px'
    badge.style.padding = '0 6px'
    badge.style.borderRadius = '999px'
    badge.style.background = 'rgba(0, 0, 0, 0.78)'
    badge.style.color = '#fff'
    badge.style.fontSize = '12px'
    badge.style.fontWeight = '700'
    badge.style.display = 'flex'
    badge.style.alignItems = 'center'
    badge.style.justifyContent = 'center'
    badge.style.lineHeight = '20px'
    previewShell.appendChild(badge)
  }

  document.body.appendChild(previewShell)
  activeDragPreview = previewShell
  return previewShell
}

export function readGalleryAssetDragIds(event: DragEvent): number[] {
  const dataTransfer = event.dataTransfer
  if (!dataTransfer) {
    return []
  }

  // drop 阶段优先读自定义 MIME，格式最稳定。
  const typedPayload = dataTransfer.getData(GALLERY_ASSET_DRAG_MIME)
  if (typedPayload) {
    return parseAssetIds(typedPayload)
  }

  // 兜底兼容：某些环境可能只保留 text/plain。
  const fallbackPayload = dataTransfer.getData('text/plain')
  return fallbackPayload
    .split(',')
    .map((value) => Number(value.trim()))
    .filter((id) => Number.isInteger(id) && id > 0)
}

export function hasGalleryAssetDragIds(event: DragEvent): boolean {
  const dataTransfer = event.dataTransfer
  if (!dataTransfer) {
    return false
  }

  // dragover/dragenter 阶段很多浏览器拿不到 getData，只能靠 types 判断可放置性。
  const types = Array.from(dataTransfer.types ?? [])
  return types.includes(GALLERY_ASSET_DRAG_MIME) || types.includes('text/plain')
}

export function useGalleryDragPayload() {
  const store = useGalleryStore()

  function resolveDragAssetIds(primaryAssetId: number): number[] {
    // 交互语义：若拖拽项已在当前选择集里，则拖“整个选择集”；否则仅拖当前项。
    const selectedIds = Array.from(store.selection.selectedIds)
    if (selectedIds.length > 0 && store.selection.selectedIds.has(primaryAssetId)) {
      return selectedIds
    }
    return [primaryAssetId]
  }

  function prepareAssetDrag(event: DragEvent, primaryAssetId: number): number[] {
    const dataTransfer = event.dataTransfer
    if (!dataTransfer) {
      return []
    }

    const assetIds = resolveDragAssetIds(primaryAssetId)
    const serialized = JSON.stringify(assetIds)
    // 同时写入自定义 MIME 和 text/plain，提升跨组件/浏览器实现差异下的稳定性。
    dataTransfer.effectAllowed = 'move'
    dataTransfer.setData(GALLERY_ASSET_DRAG_MIME, serialized)
    dataTransfer.setData('text/plain', assetIds.join(','))

    // 拖拽源优先定位缩略图容器，让预览在不同视图下表现更一致。
    const currentTarget = event.currentTarget
    if (currentTarget instanceof HTMLElement) {
      const previewSource =
        (currentTarget.querySelector('[data-asset-thumbnail]') as HTMLElement | null) ??
        currentTarget
      cleanupDragPreview()
      const preview = createDragPreview(previewSource, assetIds.length)
      // setDragImage 的 x/y 是“光标热点”坐标：x 越大预览越往左，所以这里用负值让徽标往右。
      const isListRowDrag = !!currentTarget.closest('[data-asset-list-row]')
      dataTransfer.setDragImage(preview, isListRowDrag ? -12 : -1, -1)
      window.addEventListener('dragend', cleanupDragPreview, { once: true })
    }

    return assetIds
  }

  return {
    prepareAssetDrag,
  }
}
