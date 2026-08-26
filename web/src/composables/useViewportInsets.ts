import { onBeforeUnmount, onMounted } from 'vue'

const KEYBOARD_INSET_THRESHOLD = 100
const KEYBOARD_SETTLE_DELAYS_MS = [0, 80, 180, 360, 700]
const KEYBOARD_INSET_VARIABLE = '--app-keyboard-bottom'

const NON_TEXT_INPUT_TYPES = new Set([
  'button',
  'checkbox',
  'color',
  'file',
  'hidden',
  'image',
  'radio',
  'range',
  'reset',
  'submit',
])

function isKeyboardEditableElement(element: Element | null): boolean {
  if (element instanceof HTMLTextAreaElement) {
    return !element.disabled && !element.readOnly
  }

  if (element instanceof HTMLInputElement) {
    return !element.disabled && !element.readOnly && !NON_TEXT_INPUT_TYPES.has(element.type)
  }

  return element instanceof HTMLElement && element.isContentEditable
}

/**
 * 把移动浏览器键盘造成的视觉视口遮挡同步到 CSS。
 *
 * safe-area 由 CSS env() 负责；这里仅计算键盘部分，避免把两种遮挡重复叠加。
 */
export function useViewportInsets() {
  let clearListeners: (() => void) | undefined
  let settleTimers: number[] = []

  onMounted(() => {
    const root = document.documentElement
    let safeAreaBottomPx = 0

    const measureSafeAreaBottom = () => {
      const probe = document.createElement('div')
      probe.style.cssText =
        'position:fixed;left:-9999px;top:0;width:0;height:0;' +
        'padding-bottom:env(safe-area-inset-bottom,0px);' +
        'visibility:hidden;pointer-events:none'
      document.body.appendChild(probe)
      safeAreaBottomPx = parseFloat(getComputedStyle(probe).paddingBottom) || 0
      probe.remove()
    }

    const setKeyboardInset = (inset: number) => {
      root.style.setProperty(KEYBOARD_INSET_VARIABLE, `${Math.round(Math.max(0, inset))}px`)
    }

    const updateViewport = () => {
      const viewport = window.visualViewport
      if (!viewport) {
        setKeyboardInset(0)
        return
      }

      // iOS 在没有键盘时也可能让 visualViewport 少掉 Home Indicator 高度，先扣除它。
      const rawInset = window.innerHeight - viewport.height - viewport.offsetTop
      const candidateInset = rawInset - safeAreaBottomPx
      setKeyboardInset(candidateInset >= KEYBOARD_INSET_THRESHOLD ? candidateInset : 0)
    }

    const clearSettleTimers = () => {
      settleTimers.forEach((timer) => window.clearTimeout(timer))
      settleTimers = []
    }

    const syncAfterFocusSettles = () => {
      clearSettleTimers()
      settleTimers = KEYBOARD_SETTLE_DELAYS_MS.map((delay) =>
        window.setTimeout(() => {
          if (isKeyboardEditableElement(document.activeElement)) {
            updateViewport()
          } else {
            setKeyboardInset(0)
          }
        }, delay)
      )
    }

    const handleWindowResize = () => {
      measureSafeAreaBottom()
      updateViewport()
    }

    measureSafeAreaBottom()
    updateViewport()

    const viewport = window.visualViewport
    viewport?.addEventListener('resize', updateViewport)
    viewport?.addEventListener('scroll', updateViewport)
    window.addEventListener('resize', handleWindowResize)
    window.addEventListener('pageshow', syncAfterFocusSettles)
    document.addEventListener('focusin', syncAfterFocusSettles)
    document.addEventListener('focusout', syncAfterFocusSettles)
    document.addEventListener('visibilitychange', syncAfterFocusSettles)

    clearListeners = () => {
      clearSettleTimers()
      viewport?.removeEventListener('resize', updateViewport)
      viewport?.removeEventListener('scroll', updateViewport)
      window.removeEventListener('resize', handleWindowResize)
      window.removeEventListener('pageshow', syncAfterFocusSettles)
      document.removeEventListener('focusin', syncAfterFocusSettles)
      document.removeEventListener('focusout', syncAfterFocusSettles)
      document.removeEventListener('visibilitychange', syncAfterFocusSettles)
      setKeyboardInset(0)
    }
  })

  onBeforeUnmount(() => {
    clearListeners?.()
  })
}
