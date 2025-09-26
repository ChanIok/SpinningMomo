import { useEffect, useCallback, useMemo } from 'react'
import { useGallerySelection } from './useGallerySelection'
import { useGalleryView } from './useGalleryView'
import { useGalleryStore } from '@/lib/gallery/galleryStore'

type KeyboardHandler = (event: KeyboardEvent) => void
type KeyCombination = string // e.g., 'Ctrl+a', 'Shift+ArrowLeft'

interface UseGalleryKeyboardOptions {
  enableGlobalShortcuts?: boolean
  columnsPerRow?: number
}

export function useGalleryKeyboard(options: UseGalleryKeyboardOptions = {}) {
  const { enableGlobalShortcuts = true, columnsPerRow } = options

  // 获取相关 hooks
  const selection = useGallerySelection()
  const view = useGalleryView()
  const lightboxIsOpen = useGalleryStore((state) => state.lightbox.isOpen)

  // 将键盘事件转换为组合键字符串
  const getKeyCombo = useCallback((event: KeyboardEvent): string => {
    const parts: string[] = []

    if (event.ctrlKey || event.metaKey) parts.push('Ctrl')
    if (event.altKey) parts.push('Alt')
    if (event.shiftKey) parts.push('Shift')
    parts.push(event.key)

    return parts.join('+')
  }, [])

  // Gallery 特定的快捷键处理器
  const galleryKeyHandlers = useMemo((): Record<KeyCombination, KeyboardHandler> => {
    const selectionHandlers = selection.getKeyboardHandlers(columnsPerRow)

    return {
      // 选择相关快捷键
      ...selectionHandlers,

      // 视图相关快捷键
      '1': (e) => {
        e.preventDefault()
        view.switchViewMode('masonry')
      },
      '2': (e) => {
        e.preventDefault()
        view.switchViewMode('grid')
      },
      '3': (e) => {
        e.preventDefault()
        view.switchViewMode('list')
      },
      '4': (e) => {
        e.preventDefault()
        view.switchViewMode('adaptive')
      },

      // 尺寸调整
      '+': (e) => {
        e.preventDefault()
        if (view.canIncreaseSize) {
          view.increaseSize()
        }
      },
      '=': (e) => {
        // = 键（不需要按 Shift）
        e.preventDefault()
        if (view.canIncreaseSize) {
          view.increaseSize()
        }
      },
      '-': (e) => {
        e.preventDefault()
        if (view.canDecreaseSize) {
          view.decreaseSize()
        }
      },

      // 刷新
      F5: (e) => {
        e.preventDefault()
        // 这里可以触发刷新逻辑
      },
      'Ctrl+r': (e) => {
        e.preventDefault()
        // 这里可以触发刷新逻辑
      },

      // 类型筛选快捷键
      'Ctrl+1': (e) => {
        e.preventDefault()
        selection.selectAllPhotos()
      },
      'Ctrl+2': (e) => {
        e.preventDefault()
        selection.selectAllVideos()
      },
      'Ctrl+3': (e) => {
        e.preventDefault()
        selection.selectAllLivePhotos()
      },

      // 反选
      'Ctrl+i': (e) => {
        e.preventDefault()
        selection.invertSelection()
      },

      // 删除键 - 删除选中的资产
      Delete: (e) => {
        if (selection.hasSelection) {
          e.preventDefault()
          // 这里可以触发删除确认对话框
          console.log(
            'Delete selected assets:',
            selection.selectedAssets.map((a) => a.filename)
          )
        }
      },
    }
  }, [selection, view, columnsPerRow])

  // 主键盘事件处理器
  const handleKeyDown = useCallback(
    (event: KeyboardEvent) => {
      // 如果 Lightbox 开启，不处理 Gallery 的快捷键（Lightbox 有自己的处理器）
      if (lightboxIsOpen) return

      // 如果焦点在输入框中，不处理快捷键
      const target = event.target as HTMLElement
      if (
        target &&
        (target.tagName === 'INPUT' ||
          target.tagName === 'TEXTAREA' ||
          target.contentEditable === 'true')
      ) {
        return
      }

      const keyCombo = getKeyCombo(event)
      const handler = galleryKeyHandlers[keyCombo]

      if (handler) {
        handler(event)
      }
    },
    [lightboxIsOpen, getKeyCombo, galleryKeyHandlers]
  )

  // 注册全局键盘事件监听
  useEffect(() => {
    if (enableGlobalShortcuts) {
      document.addEventListener('keydown', handleKeyDown, { capture: true })
      return () => document.removeEventListener('keydown', handleKeyDown, { capture: true })
    }
  }, [enableGlobalShortcuts, handleKeyDown])

  // 获取帮助信息 - 显示所有可用的快捷键
  const getKeyboardShortcuts = useMemo(() => {
    return [
      // 选择操作
      {
        category: 'Selection',
        shortcuts: [
          { key: 'Ctrl+A', description: 'Select all assets' },
          { key: 'Ctrl+I', description: 'Invert selection' },
          { key: 'Escape', description: 'Clear selection' },
          { key: 'Space', description: 'Toggle selection of active asset' },
          { key: 'Ctrl+1/2/3', description: 'Select by type (Photos/Videos/Live Photos)' },
        ],
      },

      // 导航
      {
        category: 'Navigation',
        shortcuts: [
          { key: 'Arrow Keys', description: 'Navigate through assets' },
          { key: 'Shift + Arrow Keys', description: 'Extend selection while navigating' },
        ],
      },

      // 视图操作
      {
        category: 'View',
        shortcuts: [
          { key: '1/2/3/4', description: 'Switch view mode (Masonry/Grid/List/Adaptive)' },
          { key: '+/=', description: 'Increase thumbnail size' },
          { key: '-', description: 'Decrease thumbnail size' },
        ],
      },

      // 其他操作
      {
        category: 'Actions',
        shortcuts: [
          { key: 'F5 / Ctrl+R', description: 'Refresh gallery' },
          { key: 'Delete', description: 'Delete selected assets' },
        ],
      },
    ]
  }, [])

  // 检查特定快捷键是否可用
  const isShortcutAvailable = useCallback(
    (keyCombo: KeyCombination): boolean => {
      return keyCombo in galleryKeyHandlers
    },
    [galleryKeyHandlers]
  )

  // 手动触发快捷键
  const triggerShortcut = useCallback(
    (keyCombo: KeyCombination) => {
      const handler = galleryKeyHandlers[keyCombo]
      if (handler) {
        // 创建一个模拟的 KeyboardEvent
        const mockEvent = new KeyboardEvent('keydown', {
          key: keyCombo.split('+').pop() || '',
          ctrlKey: keyCombo.includes('Ctrl'),
          altKey: keyCombo.includes('Alt'),
          shiftKey: keyCombo.includes('Shift'),
          metaKey: keyCombo.includes('Ctrl'), // macOS 兼容
        })
        handler(mockEvent)
      }
    },
    [galleryKeyHandlers]
  )

  return {
    // 快捷键信息
    getKeyboardShortcuts,
    isShortcutAvailable,

    // 手动控制
    triggerShortcut,

    // 实用工具
    getKeyCombo,

    // 状态
    isEnabled: enableGlobalShortcuts,
    isLightboxOpen: lightboxIsOpen,
  }
}
