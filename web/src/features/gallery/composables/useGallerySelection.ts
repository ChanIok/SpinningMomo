import { computed } from 'vue'
import { useGalleryStore } from '../store'
import type { Asset } from '../types'

/**
 * Gallery选择管理 Composable
 * 负责资产的选择交互逻辑：单选、多选、范围选择等
 */
export function useGallerySelection() {
  const store = useGalleryStore()

  // ============= 选择状态 =============
  const selectedIds = computed(() => store.selection.selectedIds)
  const selectedCount = computed(() => store.selectedCount)
  const hasSelection = computed(() => store.hasSelection)
  const lastSelectedId = computed(() => store.selection.lastSelectedId)

  // ============= 选择操作 =============

  /**
   * 选择单个资产
   * @param id 资产ID
   * @param selected 是否选中
   * @param multi 是否多选模式
   */
  function selectAsset(id: number, selected: boolean, multi = false) {
    store.selectAsset(id, selected, multi)
  }

  /**
   * 切换资产选择状态
   * @param id 资产ID
   * @param multi 是否多选模式
   */
  function toggleAsset(id: number, multi = false) {
    const isSelected = selectedIds.value.has(id)
    selectAsset(id, !isSelected, multi)
  }

  /**
   * 清空选择
   */
  function clearSelection() {
    store.clearSelection()
    // 清除选择时，也清除详情焦点
    store.clearDetailsFocus()
  }

  /**
   * 范围选择：从 lastSelectedId 到 targetId 之间的所有资产
   * @param targetId 目标资产ID
   * @param assets 当前显示的资产列表
   */
  function selectRange(targetId: number, assets: Asset[]) {
    const lastId = lastSelectedId.value
    if (!lastId) {
      // 如果没有上次选择的资产，只选择当前资产
      selectAsset(targetId, true, true)
      return
    }

    // 找到两个资产在列表中的索引
    const lastIndex = assets.findIndex((asset) => asset.id === lastId)
    const targetIndex = assets.findIndex((asset) => asset.id === targetId)

    if (lastIndex === -1 || targetIndex === -1) {
      // 如果找不到索引，只选择当前资产
      selectAsset(targetId, true, true)
      return
    }

    // 确定选择范围
    const startIndex = Math.min(lastIndex, targetIndex)
    const endIndex = Math.max(lastIndex, targetIndex)

    // 选择范围内的所有资产
    for (let i = startIndex; i <= endIndex; i++) {
      const asset = assets[i]
      if (asset) {
        selectAsset(asset.id, true, true)
      }
    }
  }

  /**
   * 反选：切换所有资产的选择状态
   * @param assets 当前显示的资产列表
   */
  function invertSelection(assets: Asset[]) {
    assets.forEach((asset) => {
      const isSelected = selectedIds.value.has(asset.id)
      selectAsset(asset.id, !isSelected, true)
    })
  }

  /**
   * 通过类型选择资产
   * @param type 资产类型
   * @param assets 当前显示的资产列表
   */
  function selectByType(type: Asset['type'], assets: Asset[]) {
    const assetsOfType = assets.filter((asset) => asset.type === type)
    assetsOfType.forEach((asset) => {
      selectAsset(asset.id, true, true)
    })
  }

  // ============= 交互处理 =============

  /**
   * 处理资产点击事件
   * @param asset 被点击的资产
   * @param event 鼠标事件
   * @param assets 当前显示的资产列表
   */
  function handleAssetClick(asset: Asset, event: MouseEvent, assets: Asset[]) {
    if (event.shiftKey) {
      // Shift + 点击：范围选择
      selectRange(asset.id, assets)
      // Shift 多选后，显示批量操作面板
      if (store.selectedCount > 1) {
        store.setDetailsFocus({ type: 'batch' })
      }
    } else if (event.ctrlKey || event.metaKey) {
      // Ctrl/Cmd + 点击：切换选择状态
      toggleAsset(asset.id, true)
      // Ctrl 多选后，判断选中数量
      if (store.selectedCount > 1) {
        store.setDetailsFocus({ type: 'batch' })
      } else if (store.selectedCount === 1) {
        store.setDetailsFocus({ type: 'asset', asset })
      }
    } else {
      // 普通点击：单选
      selectAsset(asset.id, true, false)
      // 单选时显示资产详情
      store.setDetailsFocus({ type: 'asset', asset })
    }
  }

  /**
   * 处理资产双击事件
   * @param asset 被双击的资产
   * @param _event 鼠标事件
   */
  function handleAssetDoubleClick(asset: Asset, _event: MouseEvent) {
    // 可以在这里触发其他操作，比如预览
    console.log('双击资产:', asset.name)
  }

  /**
   * 处理资产右键菜单事件
   * @param asset 被右键的资产
   * @param _event 鼠标事件
   */
  function handleAssetContextMenu(asset: Asset, _event: MouseEvent) {
    // 如果右键的资产没有被选中，则选中它
    if (!selectedIds.value.has(asset.id)) {
      selectAsset(asset.id, true, false)
    }

    // TODO: 显示上下文菜单
    console.log('右键菜单:', asset.name, '选中数量:', selectedCount.value)
  }

  // ============= 键盘快捷键 =============

  /**
   * 处理键盘选择
   * @param direction 方向：'up' | 'down' | 'left' | 'right'
   * @param assets 当前显示的资产列表
   * @param columnCount 每行列数
   * @param extend 是否扩展选择（Shift键按下）
   */
  function handleKeyboardSelection(
    direction: 'up' | 'down' | 'left' | 'right',
    assets: Asset[],
    columnCount: number,
    extend = false
  ) {
    // 通过 lastSelectedId 确定当前焦点
    const currentId = lastSelectedId.value
    if (!currentId || assets.length === 0) {
      // 如枟没有当前选中，选择第一个
      if (assets[0]) {
        selectAsset(assets[0].id, true, false)
        store.setDetailsFocus({ type: 'asset', asset: assets[0] })
      }
      return
    }

    const currentIndex = assets.findIndex((asset) => asset.id === currentId)
    if (currentIndex === -1) return

    let nextIndex = currentIndex

    switch (direction) {
      case 'left':
        nextIndex = Math.max(0, currentIndex - 1)
        break
      case 'right':
        nextIndex = Math.min(assets.length - 1, currentIndex + 1)
        break
      case 'up':
        nextIndex = Math.max(0, currentIndex - columnCount)
        break
      case 'down':
        nextIndex = Math.min(assets.length - 1, currentIndex + columnCount)
        break
    }

    const nextAsset = assets[nextIndex]
    if (nextAsset) {
      if (extend) {
        // Shift + 方向键：扩展选择
        selectRange(nextAsset.id, assets)
      } else {
        // 普通方向键：单选
        selectAsset(nextAsset.id, true, false)
        store.setDetailsFocus({ type: 'asset', asset: nextAsset })
      }
    }
  }

  // ============= 工具方法 =============

  /**
   * 检查资产是否被选中
   * @param id 资产ID
   */
  function isAssetSelected(id: number): boolean {
    return selectedIds.value.has(id)
  }

  /**
   * 获取下一个资产ID
   * @param assets 资产列表
   * @param direction 方向
   */
  function getNextAssetId(
    assets: Asset[],
    direction: 'next' | 'prev' = 'next'
  ): number | undefined {
    const currentId = lastSelectedId.value
    if (!currentId) return assets[0]?.id

    const currentIndex = assets.findIndex((asset) => asset.id === currentId)
    if (currentIndex === -1) return assets[0]?.id

    if (direction === 'next') {
      const nextIndex = (currentIndex + 1) % assets.length
      return assets[nextIndex]?.id
    } else {
      const prevIndex = (currentIndex - 1 + assets.length) % assets.length
      return assets[prevIndex]?.id
    }
  }

  return {
    // 状态
    selectedIds,
    selectedCount,
    hasSelection,
    lastSelectedId,

    // 基本操作
    selectAsset,
    toggleAsset,
    clearSelection,

    // 高级操作
    selectRange,
    invertSelection,
    selectByType,

    // 事件处理
    handleAssetClick,
    handleAssetDoubleClick,
    handleAssetContextMenu,
    handleKeyboardSelection,

    // 工具方法
    isAssetSelected,
    getNextAssetId,
  }
}
