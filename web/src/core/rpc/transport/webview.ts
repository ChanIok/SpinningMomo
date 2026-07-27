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
 * 创建 WebView 传输方法集合
 */
export function createWebViewTransport(): TransportMethods {
  const pendingRequests = new Map<string | number, PendingRequest>()
  const eventHandlers = new Map<string, Set<(params: unknown) => void>>()
  let nextId = 1
  const isDebugMode = import.meta.env.DEV
  let isInitialized = false

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

  // 通过 WebView2 侧信道发送 DOM 对象，JSON 消息仍沿用普通 RPC 协议。
  function postMessageWithAdditionalObjects(
    message: JsonRpcRequest,
    additionalObjects: object[]
  ): void {
    const webview = window.chrome?.webview
    if (!webview?.postMessageWithAdditionalObjects) {
      throw new JsonRpcError(
        JsonRpcErrorCode.WEBVIEW_NOT_AVAILABLE,
        'WebView2 additional objects are not available'
      )
    }
    webview.postMessageWithAdditionalObjects(message, additionalObjects)
  }

  function handleResponse(response: JsonRpcResponse): void {
    const pendingRequest = pendingRequests.get(response.id)
    if (!pendingRequest) return

    const { resolve, reject, timeout } = pendingRequest
    if (timeout) clearTimeout(timeout)
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
    if (typeof message !== 'object' || !message) return false

    const msg = message as Record<string, unknown>

    if (msg.jsonrpc !== '2.0') return false

    if ('method' in msg && typeof msg.method === 'string') return true

    if ('id' in msg && ('result' in msg || 'error' in msg)) return true

    return false
  }

  function handleMessage(event: MessageEvent): void {
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

  // 为普通消息和附加对象消息复用同一套请求 ID、超时和 Promise 生命周期。
  function callRequest<T>(
    method: string,
    params: unknown,
    timeout: number,
    send: (request: JsonRpcRequest) => void
  ): Promise<T> {
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

      let timeoutHandle: ReturnType<typeof setTimeout> | null = null
      if (timeout > 0) {
        timeoutHandle = setTimeout(() => {
          pendingRequests.delete(id)
          reject(
            new JsonRpcError(JsonRpcErrorCode.TIMEOUT, `Request timeout: ${method}`, {
              method,
              timeout,
            })
          )
        }, timeout)
      }

      pendingRequests.set(id, {
        resolve: (value: unknown) => resolve(value as T),
        reject,
        timeout: timeoutHandle,
      })

      // 先登记 pending 再发送，确保同步抛错和快速响应都能正确收敛。
      try {
        send(request)
        if (isDebugMode) console.log('[WebView RPC]', 'RPC call:', method, params)
      } catch (error) {
        if (timeoutHandle) clearTimeout(timeoutHandle)
        pendingRequests.delete(id)
        reject(error)
      }
    })
  }

  // 返回 TransportMethods 接口实现
  return {
    call: async <T>(method: string, params?: unknown, timeout = 10000): Promise<T> => {
      return callRequest<T>(method, params, timeout, postMessage)
    },

    callWithAdditionalObjects: async <T>(
      method: string,
      params: unknown,
      additionalObjects: object[],
      timeout = 10000
    ): Promise<T> => {
      return callRequest<T>(method, params, timeout, (request) =>
        postMessageWithAdditionalObjects(request, additionalObjects)
      )
    },

    on: (method: string, handler: (params: unknown) => void): void => {
      if (!eventHandlers.has(method)) {
        eventHandlers.set(method, new Set())
      }
      eventHandlers.get(method)!.add(handler)
      if (isDebugMode) console.log('[WebView RPC]', 'Event listener added:', method)
    },

    off: (method: string, handler: (params: unknown) => void): void => {
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
          for (const [, request] of pendingRequests) {
            if (request.timeout) clearTimeout(request.timeout)
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
      for (const [, request] of pendingRequests) {
        if (request.timeout) clearTimeout(request.timeout)
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

    getStats: (): TransportStats => {
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
