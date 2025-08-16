import type { TransportMethods } from './types'
import {
  JsonRpcError,
  JsonRpcErrorCode,
  type PendingRequest,
  type JsonRpcRequest,
  type JsonRpcResponse,
  type JsonRpcNotification,
  type TransportStats,
} from '../types'

/**
 * 创建WebView传输方法集合
 */
export function createWebViewTransport(): TransportMethods {
  // 传输特有的状态 - 从原webview-rpc.ts复制
  const pendingRequests = new Map<string | number, PendingRequest>()
  const eventHandlers = new Map<string, Set<(params: unknown) => void>>()
  let nextId = 1
  const isDebugMode = import.meta.env.DEV
  let isInitialized = false

  // 内部辅助函数 - 从原webview-rpc.ts复制并修改
  function isWebViewAvailable(): boolean {
    return typeof window !== 'undefined' && !!window.chrome?.webview
  }

  function postMessage(message: JsonRpcRequest | JsonRpcNotification): void {
    if (isWebViewAvailable() && window.chrome?.webview) {
      window.chrome.webview.postMessage(message)
    } else if (isDebugMode) {
      console.log('[WebView RPC]', 'Mock message (WebView2 not available):', message)
    } else {
      throw new JsonRpcError(JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE, 'WebView2 not available')
    }
  }

  function handleResponse(response: JsonRpcResponse): void {
    // 从原webview-rpc.ts复制handleResponse函数的完整逻辑
    const pendingRequest = pendingRequests.get(response.id)
    if (!pendingRequest) return

    const { resolve, reject, timeout } = pendingRequest
    clearTimeout(timeout)
    pendingRequests.delete(response.id)

    if (response.error) {
      const error = new JsonRpcError(
        response.error.code as JsonRpcErrorCode,
        response.error.message,
        response.error.data
      )
      reject(error)
      if (isDebugMode) console.log('[WebView RPC]', 'RPC error:', response.error)
    } else {
      resolve(response.result)
      if (isDebugMode) console.log('[WebView RPC]', 'RPC response:', response.id, response.result)
    }
  }

  function handleNotification(notification: JsonRpcNotification): void {
    // 从原webview-rpc.ts复制handleNotification函数的完整逻辑
    const handlers = eventHandlers.get(notification.method)
    if (handlers && handlers.size > 0) {
      handlers.forEach((handler) => {
        try {
          handler(notification.params)
        } catch (error) {
          console.error(`Error in event handler for ${notification.method}:`, error)
        }
      })
      if (isDebugMode)
        console.log('[WebView RPC]', 'Event received:', notification.method, notification.params)
    } else {
      if (isDebugMode) console.log('[WebView RPC]', 'No handlers for event:', notification.method)
    }
  }

  function isValidJsonRpcMessage(message: unknown): boolean {
    // 从原webview-rpc.ts复制isValidJsonRpcMessage函数的完整逻辑
    if (typeof message !== 'object' || !message) return false

    const msg = message as Record<string, unknown>

    if (msg.jsonrpc !== '2.0') return false

    if ('method' in msg && typeof msg.method === 'string') return true

    if ('id' in msg && ('result' in msg || 'error' in msg)) return true

    return false
  }

  function handleMessage(event: MessageEvent): void {
    // 从原webview-rpc.ts复制handleMessage函数的完整逻辑
    try {
      const message = event.data

      if (!isValidJsonRpcMessage(message)) {
        if (isDebugMode) console.log('[WebView RPC]', 'Invalid JSON-RPC message:', message)
        return
      }

      if ('id' in message && pendingRequests.has(message.id)) {
        handleResponse(message as JsonRpcResponse)
      } else if ('method' in message && !('id' in message)) {
        handleNotification(message as JsonRpcNotification)
      }
    } catch (error) {
      console.error('Failed to handle WebView message:', error)
    }
  }

  // 返回TransportMethods接口实现
  return {
    call: async <T>(method: string, params?: unknown, timeout = 10000): Promise<T> => {
      // 从原webview-rpc.ts复制call函数的完整逻辑
      return new Promise((resolve, reject) => {
        if (!isWebViewAvailable()) {
          reject(new JsonRpcError(JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE, 'WebView2 not available'))
          return
        }

        const id = nextId++
        const request: JsonRpcRequest = {
          jsonrpc: '2.0',
          method,
          params,
          id,
        }

        const timeoutHandle = setTimeout(() => {
          pendingRequests.delete(id)
          reject(
            new JsonRpcError(JsonRpcErrorCode.TIMEOUT, `Request timeout: ${method}`, {
              method,
              timeout,
            })
          )
        }, timeout)

        pendingRequests.set(id, {
          resolve: (value: unknown) => resolve(value as T),
          reject,
          timeout: timeoutHandle,
        })

        try {
          postMessage(request)
          if (isDebugMode) console.log('[WebView RPC]', 'RPC call:', method, params)
        } catch (error) {
          clearTimeout(timeoutHandle)
          pendingRequests.delete(id)
          reject(error)
        }
      })
    },

    on: (method: string, handler: (params: unknown) => void): void => {
      // 从原webview-rpc.ts复制on函数的完整逻辑
      if (!eventHandlers.has(method)) {
        eventHandlers.set(method, new Set())
      }
      eventHandlers.get(method)!.add(handler)
      if (isDebugMode) console.log('[WebView RPC]', 'Event listener added:', method)
    },

    off: (method: string, handler: (params: unknown) => void): void => {
      // 从原webview-rpc.ts复制off函数的完整逻辑
      const handlers = eventHandlers.get(method)
      if (handlers) {
        handlers.delete(handler)
        if (handlers.size === 0) {
          eventHandlers.delete(method)
        }
      }
      if (isDebugMode) console.log('[WebView RPC]', 'Event listener removed:', method)
    },

    initialize: async (): Promise<void> => {
      // 从原webview-rpc.ts复制initializeRPC函数的完整逻辑
      if (isInitialized) {
        if (isDebugMode) console.log('[WebView RPC]', 'RPC already initialized.')
        return
      }

      if (isWebViewAvailable() && window.chrome?.webview) {
        window.chrome.webview.addEventListener('message', handleMessage)
        isInitialized = true
        if (isDebugMode) console.log('[WebView RPC]', 'WebView RPC initialized')
      } else if (isDebugMode) {
        console.log('[WebView RPC]', 'WebView2 not available, running in mock mode')
      }

      // 确保在页面卸载时清理资源
      if (typeof window !== 'undefined') {
        window.addEventListener('beforeunload', () => {
          // We need to access the transport's dispose method here
          // But since we're in a closure, we need to reference it differently
          // For now, we'll just do the cleanup directly
          for (const [, request] of pendingRequests) {
            clearTimeout(request.timeout)
            request.reject(
              new JsonRpcError(JsonRpcErrorCode.INTERNAL_ERROR, 'WebView RPC disposed')
            )
          }
          pendingRequests.clear()
          eventHandlers.clear()
        })
      }
    },

    dispose: (): void => {
      // 从原webview-rpc.ts复制dispose函数的完整逻辑
      for (const [, request] of pendingRequests) {
        clearTimeout(request.timeout)
        request.reject(new JsonRpcError(JsonRpcErrorCode.INTERNAL_ERROR, 'WebView RPC disposed'))
      }
      pendingRequests.clear()
      eventHandlers.clear()

      if (isWebViewAvailable() && window.chrome?.webview) {
        window.chrome.webview.removeEventListener('message', handleMessage)
      }

      isInitialized = false
      if (isDebugMode) console.log('[WebView RPC]', 'WebView RPC disposed')
    },

    isAvailable: (): boolean => {
      return isWebViewAvailable()
    },

    getStats: (): TransportStats => {
      // 从原webview-rpc.ts复制getStats函数的完整逻辑
      return {
        pendingRequests: pendingRequests.size,
        eventHandlers: Array.from(eventHandlers.entries()).map(([method, handlers]) => ({
          method,
          handlerCount: handlers.size,
        })),
        isConnected: isWebViewAvailable(),
        transportType: 'webview',
      }
    },
  }
}
