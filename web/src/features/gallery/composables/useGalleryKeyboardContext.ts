import { inject, provide, ref, watch, type InjectionKey } from 'vue'
import { useEventListener } from '@vueuse/core'
import { useGalleryStore } from '../store'
import { useGalleryOverlayHistory } from './useGalleryOverlayHistory'

export type GalleryKeyboardScope = 'gallery' | 'lightbox' | 'details'

export interface GalleryKeyboardContext {
  canHandleShortcut: (event: KeyboardEvent, scope: GalleryKeyboardScope) => boolean
  hasActiveOverlay: (target?: EventTarget | null) => boolean
  setActiveScope: (scope: GalleryKeyboardScope | null) => void
}

const keyboardContextKey: InjectionKey<GalleryKeyboardContext> = Symbol('galleryKeyboardContext')

const OVERLAY_SELECTOR =
  '[role="menu"], [role="dialog"], [role="alertdialog"], [data-slot="popover-content"], ' +
  '[data-slot="dropdown-menu-content"], [data-slot="context-menu-content"], ' +
  '[data-slot="context-menu-sub-content"], [data-slot="dialog-overlay"], ' +
  '[data-slot="alert-dialog-overlay"]'

const OPEN_OVERLAY_SELECTOR =
  '[data-slot="popover-content"][data-state="open"], ' +
  '[data-slot="dropdown-menu-content"][data-state="open"], ' +
  '[data-slot="context-menu-content"][data-state="open"], ' +
  '[data-slot="context-menu-sub-content"][data-state="open"], ' +
  '[data-slot="dialog-content"][data-state="open"], ' +
  '[data-slot="alert-dialog-content"][data-state="open"]'

export function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false
  }

  return target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)
}

export function isSelectAllShortcut(event: KeyboardEvent): boolean {
  return (
    (event.ctrlKey || event.metaKey) &&
    !event.shiftKey &&
    !event.altKey &&
    event.key.toLowerCase() === 'a'
  )
}

function parseKeyboardScope(value: string | null | undefined): GalleryKeyboardScope | null {
  if (value === 'gallery' || value === 'lightbox' || value === 'details') {
    return value
  }
  return null
}

/** 快捷键归属按语义区域追踪，弹层交互不会覆盖原有操作区域。 */
export function provideGalleryKeyboardContext() {
  const store = useGalleryStore()
  const overlayHistory = useGalleryOverlayHistory()
  const activeScope = ref<GalleryKeyboardScope | null>(
    store.lightbox.isOpen ? 'lightbox' : 'gallery'
  )
  let isTabNavigation = false

  function hasActiveOverlay(target?: EventTarget | null): boolean {
    if (target instanceof Element && target.closest(OVERLAY_SELECTOR)) {
      return true
    }

    const overlay = overlayHistory.snapshot.value.overlay
    if (
      store.contextMenu.isOpen ||
      store.moveToFolderDialogOpen ||
      store.deleteAssetsDialog.open ||
      store.preferencesDialogOpen ||
      (overlay !== null && overlay !== 'selection' && overlay !== 'lightbox')
    ) {
      return true
    }

    return Boolean(document.querySelector(OPEN_OVERLAY_SELECTOR))
  }

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
    activeScope.value = parseKeyboardScope(scope)
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
    (): GalleryKeyboardScope =>
      store.lightbox.isOpen
        ? overlayHistory.snapshot.value.overlay === 'lightbox-details'
          ? 'details'
          : 'lightbox'
        : 'gallery',
    (scope) => {
      activeScope.value = scope
    }
  )

  provide(keyboardContextKey, {
    hasActiveOverlay,
    setActiveScope(scope) {
      activeScope.value = scope
    },
    canHandleShortcut(event, scope) {
      if (
        activeScope.value !== scope ||
        event.defaultPrevented ||
        event.isComposing ||
        event.altKey ||
        hasActiveOverlay(event.target)
      ) {
        return false
      }

      // 普通复制始终尊重原生文字选区，包括焦点仍留在旧控件上的情况。
      return !(
        (event.ctrlKey || event.metaKey) &&
        !event.shiftKey &&
        event.key.toLowerCase() === 'c' &&
        window.getSelection()?.toString()
      )
    },
  })
}

export function useGalleryKeyboardContext(): GalleryKeyboardContext {
  const context = inject(keyboardContextKey)
  if (!context) throw new Error('Gallery keyboard context must be provided by GalleryPage')
  return context
}
