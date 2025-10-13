import { getTransportMethods, getCurrentTransportType, resetTransport } from './core'
import type { TransportStats, TransportType } from './types'

// 导出类型
export type {
  JsonRpcRequest,
  JsonRpcResponse,
  JsonRpcNotification,
  JsonRpcErrorCode,
  TransportType,
  TransportStats,
} from './types'
export { JsonRpcError } from './types'

/**
 * 调用远程方法（请求-响应模式）
 */
export async function call<T = unknown>(
  method: string,
  params?: unknown,
  timeout = 10000
): Promise<T> {
  const transport = getTransportMethods()
  return transport.call<T>(method, params, timeout)
}

/**
 * 监听事件通知
 */
export function on(method: string, handler: (params: unknown) => void): void {
  const transport = getTransportMethods()
  transport.on(method, handler)
}

/**
 * 取消事件监听
 */
export function off(method: string, handler: (params: unknown) => void): void {
  const transport = getTransportMethods()
  transport.off(method, handler)
}

/**
 * 获取统计信息
 */
export function getStats(): TransportStats {
  const transport = getTransportMethods()
  return transport.getStats()
}

/**
 * 清理资源，在应用卸载时调用
 */
export function dispose(): void {
  const transport = getTransportMethods()
  transport.dispose()
}

/**
 * 初始化 RPC 通信，应在应用启动时调用一次
 */
export function initializeRPC(): void {
  const transport = getTransportMethods()
  // 异步初始化，不阻塞主线程
  transport.initialize().catch((error) => {
    console.error('Failed to initialize RPC transport:', error)
  })
}

/**
 * 获取当前传输类型
 */
export function getTransportType(): TransportType | null {
  return getCurrentTransportType()
}

/**
 * 检查是否连接可用
 */
export function isConnected(): boolean {
  try {
    const stats = getStats()
    return stats.isConnected
  } catch {
    return false
  }
}

// 内部使用的重置函数（测试用）
export const __internal = {
  resetTransport,
}

// --- 类型辅助工具 ---
export type RpcMethod<TParams = unknown, TResult = unknown> = {
  params: TParams
  result: TResult
}

export type RpcEventHandler<T = unknown> = (params: T) => void
