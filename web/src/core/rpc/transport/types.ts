import type { TransportStats } from '../types'

export type { TransportType } from '../types'

/**
 * 传输层统一接口
 * 所有传输实现（WebView, HTTP）必须实现此接口
 */
export interface TransportMethods {
  /**
   * 调用远程方法（请求-响应模式）
   */
  call: <T>(method: string, params?: unknown, timeout?: number) => Promise<T>

  /**
   * 监听事件通知
   */
  on: (method: string, handler: (params: unknown) => void) => void

  /**
   * 取消事件监听
   */
  off: (method: string, handler: (params: unknown) => void) => void

  /**
   * 初始化传输层
   */
  initialize: () => Promise<void>

  /**
   * 清理资源
   */
  dispose: () => void

  /**
   * 获取统计信息
   */
  getStats: () => TransportStats
}
