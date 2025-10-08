import { ref } from 'vue'
import { call } from '@/core/rpc'
import type { ApiTestRequest, ApiTestResponse, ApiTestHistory } from '../types'

export function useApiTest() {
  const loading = ref(false)
  const history = ref<ApiTestHistory[]>([])

  // 执行API测试
  const testApi = async (request: ApiTestRequest): Promise<ApiTestResponse> => {
    loading.value = true
    const startTime = Date.now()

    try {
      const result = await call(request.method, request.params)
      const duration = Date.now() - startTime

      const response: ApiTestResponse = {
        success: true,
        data: result,
        timestamp: Date.now(),
        duration,
      }

      // 添加到历史记录
      addToHistory(request, response)

      return response
    } catch (error) {
      const duration = Date.now() - startTime

      const response: ApiTestResponse = {
        success: false,
        error: {
          code: (error as any).code || -1,
          message: (error as any).message || '未知错误',
          data: (error as any).data,
        },
        timestamp: Date.now(),
        duration,
      }

      // 添加到历史记录
      addToHistory(request, response)

      return response
    } finally {
      loading.value = false
    }
  }

  // 添加到历史记录
  const addToHistory = (request: ApiTestRequest, response: ApiTestResponse) => {
    const historyItem: ApiTestHistory = {
      id: `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`,
      request,
      response,
      timestamp: Date.now(),
    }

    history.value.unshift(historyItem)

    // 限制历史记录数量
    if (history.value.length > 50) {
      history.value = history.value.slice(0, 50)
    }
  }

  // 清空历史记录
  const clearHistory = () => {
    history.value = []
  }

  // 删除特定历史记录
  const removeHistoryItem = (id: string) => {
    const index = history.value.findIndex((item) => item.id === id)
    if (index > -1) {
      history.value.splice(index, 1)
    }
  }

  // 格式化JSON参数
  const formatParams = (params: unknown): string => {
    try {
      return JSON.stringify(params, null, 2)
    } catch {
      return String(params)
    }
  }

  // 解析JSON参数
  const parseParams = (jsonString: string): unknown => {
    try {
      return JSON.parse(jsonString)
    } catch {
      return jsonString
    }
  }

  return {
    loading,
    history,
    testApi,
    clearHistory,
    removeHistoryItem,
    formatParams,
    parseParams,
  }
}
