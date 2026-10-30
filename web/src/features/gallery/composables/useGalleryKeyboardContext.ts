import { inject, provide, ref, watch, type InjectionKey } from 'vue'
import { useEventListener } from '@vueuse/core'
import { useGalleryStore } from '../store'
import { useGalleryOverlayHistory } from './useGalleryOverlayHistory'

type GalleryKeyboardScope = 'gallery' | 'lightbox'
type CanHandleShortcut = (event: KeyboardEvent, scope: GalleryKeyboardScope) => boolean

const keyboardContextKey: InjectionKey<CanHandleShortcut> = Symbol('galleryKeyboardContext')
const OVERLAY_SELECTOR =
  '[role="menu"], [role="dialog"], [role="alertdialog"], [data-slot="popover-content"], ' +
  '[data-slot="dialog-overlay"], [data-slot="alert-dialog-overlay"]'

/** 快捷键归属独立于图片选区和 DOM 焦点，菜单、弹窗不会覆盖原来的操作区域。 */
export function provideGalleryKeyboardContext() {
  const store = useGalleryStore()
  const overlayHistory = useGalleryOverlayHistory()
  const activeScope = ref<GalleryKeyboardScope | null>(store.lightbox.isOpen ? 'lightbox' : null)
  let isTabNavigation = false

  function trackInteraction(event: PointerEvent | FocusEvent) {
    const target = event.target
    if (!(target instanceof Element)) return
    if (event instanceof PointerEvent) {
      if (!event.isPrimary) return
      isTabNavigation = false
    } else if (!isTabNavigation || target === document.body) {
      // 只跟随用户的 Tab 导航，弹层自动恢复焦点不改变操作区域。
      return
    }
    if (target.closest(OVERLAY_SELECTOR)) return

    const scope = target
      .closest('[data-gallery-keyboard-scope]')
      ?.getAttribute('data-gallery-keyboard-scope')
    activeScope.value = scope === 'gallery' || scope === 'lightbox' ? scope : null
  }

  useEventListener(window, 'pointerdown', trackInteraction, { capture: true })
  useEventListener(window, 'focusin', trackInteraction, { capture: true })
  useEventListener(
    window,
    'keydown',
    (event) => {
      isTabNavigation = event.key === 'Tab'
    },
    { capture: true }
  )

  // 打开、退出暗房和详情抽屉时切换操作区域，不主动改变浏览器焦点。
  watch(
    (): GalleryKeyboardScope | null =>
      store.lightbox.isOpen
        ? overlayHistory.snapshot.value.overlay === 'lightbox-details'
          ? null
          : 'lightbox'
        : 'gallery',
    (scope) => {
      activeScope.value = scope
    }
  )

  provide(keyboardContextKey, (event, scope) => {
    const overlay = overlayHistory.snapshot.value.overlay
    if (
      activeScope.value !== scope ||
      event.defaultPrevented ||
      event.isComposing ||
      event.altKey ||
      store.contextMenu.isOpen ||
      store.moveToFolderDialogOpen ||
      store.deleteAssetsDialog.open ||
      store.preferencesDialogOpen ||
      (overlay !== null && overlay !== 'selection' && overlay !== 'lightbox') ||
      (event.target instanceof Element && event.target.closest(OVERLAY_SELECTOR))
    )
      return false

    // 普通复制始终尊重原生文字选区，包括焦点仍留在旧控件上的情况。
    return !(
      (event.ctrlKey || event.metaKey) &&
      !event.shiftKey &&
      event.key.toLowerCase() === 'c' &&
      window.getSelection()?.toString()
    )
  })
}

export function useGalleryKeyboardContext(): CanHandleShortcut {
  const canHandle = inject(keyboardContextKey)
  if (!canHandle) throw new Error('Gallery keyboard context must be provided by GalleryPage')
  return canHandle
}
