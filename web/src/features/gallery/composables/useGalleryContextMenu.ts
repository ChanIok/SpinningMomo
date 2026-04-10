import { reactive, readonly } from 'vue'
import type { Asset } from '../types'

export type GalleryContextMenuSourceView = 'grid' | 'list' | 'masonry' | 'adaptive' | 'filmstrip'

interface GalleryContextMenuState {
  isOpen: boolean
  requestToken: number
  anchorX: number
  anchorY: number
  contextAssetId?: number
  contextIndex?: number
  sourceView?: GalleryContextMenuSourceView
}

const state = reactive<GalleryContextMenuState>({
  isOpen: false,
  requestToken: 0,
  anchorX: 0,
  anchorY: 0,
  contextAssetId: undefined,
  contextIndex: undefined,
  sourceView: undefined,
})

interface OpenForAssetOptions {
  asset: Asset
  event: MouseEvent
  index: number
  sourceView: GalleryContextMenuSourceView
}

export function useGalleryContextMenu() {
  function openForAsset(options: OpenForAssetOptions) {
    const { asset, event, index, sourceView } = options
    // 右键入口统一拦截浏览器默认菜单，避免与自定义菜单重叠。
    event.preventDefault()
    event.stopPropagation()

    // 上下文信息只记录“当前语义焦点”，菜单动作仍由现有 assetActions 读取 selection 执行。
    state.contextAssetId = asset.id
    state.contextIndex = index
    state.sourceView = sourceView
    state.anchorX = event.clientX
    state.anchorY = event.clientY
    // 已开状态下先关闭，让宿主在下一拍基于新锚点“重开”，避免位置不刷新。
    if (state.isOpen) {
      state.isOpen = false
    }
    // token 仅作为“定位后重开”的信号，不承载业务状态。
    state.requestToken += 1
  }

  function setOpen(open: boolean) {
    state.isOpen = open
  }

  function close() {
    setOpen(false)
  }

  return {
    state: readonly(state),
    openForAsset,
    setOpen,
    close,
  }
}
