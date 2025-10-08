import { ref, computed } from 'vue'
import { call } from '@/core/rpc'
import type { ApiMethod } from '../types'

// 获取所有可用的API方法
export function useApiMethods() {
  const methods = ref<ApiMethod[]>([])
  const loading = ref(false)
  const error = ref<string | null>(null)

  // 按分类分组的方法
  const methodsByCategory = computed(() => {
    const grouped: Record<string, ApiMethod[]> = {}

    methods.value.forEach((method) => {
      const category = method.category || '未分类'
      if (!grouped[category]) {
        grouped[category] = []
      }
      grouped[category].push(method)
    })

    return grouped
  })

  // 获取所有方法列表
  const fetchMethods = async () => {
    loading.value = true
    error.value = null

    try {
      // 调用 system.listMethods 获取所有API方法
      // 后端返回的是包含 name 和 description 的对象数组
      const methodData = await call<Array<{ name: string; description: string }>>('system.listMethods')

      // 为每个方法创建基本信息
      const apiMethods: ApiMethod[] = methodData.map((method) => {
        // 确保name是字符串类型
        const methodName = String(method.name)
        const methodDescription = String(method.description || `API 方法: ${methodName}`)
        
        // 根据方法名推断分类
        let category = '未分类'
        if (methodName.includes('.')) {
          const parts = methodName.split('.')
          if (parts.length >= 2 && parts[0]) {
            category = parts[0]
          }
        } else if (methodName.startsWith('system')) {
          category = '系统'
        }

        return {
          name: methodName,
          category,
          description: methodDescription,
        }
      })

      methods.value = apiMethods
    } catch (err) {
      error.value = err instanceof Error ? err.message : '获取API方法失败'
      console.error('Failed to fetch API methods:', err)
    } finally {
      loading.value = false
    }
  }

  // 根据名称搜索方法
  const searchMethods = (query: string) => {
    if (!query.trim()) return methods.value

    const lowerQuery = query.toLowerCase()
    return methods.value.filter(
      (method) =>
        method.name.toLowerCase().includes(lowerQuery) ||
        method.category?.toLowerCase().includes(lowerQuery) ||
        method.description?.toLowerCase().includes(lowerQuery)
    )
  }

  // 根据分类获取方法
  const getMethodsByCategory = (category: string) => {
    return methods.value.filter((method) => method.category === category)
  }

  return {
    methods,
    methodsByCategory,
    loading,
    error,
    fetchMethods,
    searchMethods,
    getMethodsByCategory,
  }
}
