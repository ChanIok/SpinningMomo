import { toast as sonnerToast } from 'vue-sonner'
import type { ExternalToast } from 'vue-sonner'

export interface ToastOptions extends ExternalToast {
  title?: string
  description?: string
}

/**
 * Toast notification composable
 * 
 * @example
 * ```ts
 * const { toast } = useToast()
 * 
 * // Basic usage
 * toast.success('Settings saved')
 * 
 * // With description
 * toast.error('Failed to save', { description: 'Network error' })
 * 
 * // Custom duration
 * toast.info('Processing...', { duration: 5000 })
 * 
 * // With action
 * toast('Delete item?', {
 *   action: {
 *     label: 'Undo',
 *     onClick: () => console.log('Undo clicked')
 *   }
 * })
 * ```
 */
export function useToast() {
  /**
   * Show a default toast
   */
  const toast = (message: string, options?: ToastOptions) => {
    return sonnerToast(message, options)
  }

  /**
   * Show a success toast
   */
  toast.success = (message: string, options?: ToastOptions) => {
    return sonnerToast.success(message, options)
  }

  /**
   * Show an error toast
   */
  toast.error = (message: string, options?: ToastOptions) => {
    return sonnerToast.error(message, options)
  }

  /**
   * Show an info toast
   */
  toast.info = (message: string, options?: ToastOptions) => {
    return sonnerToast.info(message, options)
  }

  /**
   * Show a warning toast
   */
  toast.warning = (message: string, options?: ToastOptions) => {
    return sonnerToast.warning(message, options)
  }

  /**
   * Show a loading toast
   */
  toast.loading = (message: string, options?: ToastOptions) => {
    return sonnerToast.loading(message, options)
  }

  /**
   * Show a promise toast (automatically updates based on promise state)
   */
  toast.promise = sonnerToast.promise

  /**
   * Dismiss a toast by ID
   */
  toast.dismiss = sonnerToast.dismiss

  /**
   * Create a custom toast with full control
   */
  toast.custom = sonnerToast.custom

  return { toast }
}

// Re-export for direct usage if needed
export { toast } from 'vue-sonner'
