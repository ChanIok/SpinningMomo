import type { TransportMethods, TransportType } from './transport/types'
import { createWebViewTransport } from './transport/webview'
import { createHttpTransport } from './transport/http'
import { getCurrentEnvironment } from '@/core/env'

// 模块级状态
let currentTransport: TransportMethods | null = null
let currentTransportType: TransportType | null = null
const isDebugMode = import.meta.env.DEV

/**
 * 创建传输方法集合
 */
function createTransportMethods(type: TransportType): TransportMethods {
  switch (type) {
    case 'webview':
      return createWebViewTransport()
    case 'http':
      return createHttpTransport()
    default:
      throw new Error(`Unsupported transport type: ${type}`)
  }
}

/**
 * 确保传输已初始化
 */
function ensureTransportInitialized(): TransportMethods {
  if (!currentTransport) {
    const transportType = getCurrentEnvironment() === 'webview' ? 'webview' : 'http'
    currentTransportType = transportType
    currentTransport = createTransportMethods(transportType)

    if (isDebugMode) {
      console.log(`[RPC] Using ${transportType} transport`)
    }
  }
  return currentTransport
}

/**
 * 获取当前传输类型
 */
export function getCurrentTransportType(): TransportType | null {
  return currentTransportType
}

/**
 * 获取传输方法
 */
export function getTransportMethods(): TransportMethods {
  return ensureTransportInitialized()
}

/**
 * 重置传输（用于测试）
 */
export function resetTransport(): void {
  if (currentTransport) {
    currentTransport.dispose()
  }
  currentTransport = null
  currentTransportType = null
}
