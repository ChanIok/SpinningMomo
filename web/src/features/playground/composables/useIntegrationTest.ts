import { ref } from 'vue'
import { call } from '@/core/rpc'

// 测试场景分类
export type ScenarioCategory = 'gallery' | 'database' | 'system'

// 测试步骤
export interface TestStep {
  method: string
  params?: unknown
  description: string
}

// 测试场景定义
export interface TestScenario {
  id: string
  name: string
  description: string
  icon: string
  category: ScenarioCategory
  steps: TestStep[]
  dangerous?: boolean
}

// 执行日志
export interface ExecutionLog {
  timestamp: number
  step: string
  status: 'pending' | 'running' | 'success' | 'error'
  message: string
  data?: unknown
  error?: string
  duration?: number
}

// 执行结果
export interface ExecutionResult {
  scenarioId: string
  startTime: number
  endTime?: number
  duration?: number
  status: 'running' | 'success' | 'error'
  logs: ExecutionLog[]
  finalResult?: unknown
}

export function useIntegrationTest() {
  const executing = ref(false)
  const currentExecution = ref<ExecutionResult | null>(null)
  const executionHistory = ref<ExecutionResult[]>([])

  // 定义所有可用的测试场景
  const scenarios = ref<TestScenario[]>([
    {
      id: 'scan-photos',
      name: '扫描照片文件夹',
      description: '从游戏目录扫描照片，生成缩略图',
      icon: '📸',
      category: 'gallery',
      steps: [
        {
          method: 'plugins.infinityNikki.getGameDirectory',
          description: '获取游戏目录',
        },
        {
          method: 'gallery.scanDirectory',
          description: '扫描照片目录并生成缩略图',
        },
      ],
    },
  ])

  // 添加日志
  const addLog = (log: ExecutionLog) => {
    if (currentExecution.value) {
      currentExecution.value.logs.push(log)
    }
  }

  // 执行单个步骤
  const executeStep = async (step: TestStep, context: Record<string, unknown> = {}) => {
    const startTime = Date.now()

    addLog({
      timestamp: startTime,
      step: step.method,
      status: 'running',
      message: step.description,
    })

    try {
      let params = step.params

      // 如果是扫描目录步骤，使用之前获取的 gameDir
      if (step.method === 'gallery.scanDirectory' && context.gameDir) {
        params = {
          directory: context.gameDir,
          generateThumbnails: true,
          thumbnailShortEdge: 400,
          ignoreRules: [
            {
              pattern: '**',
              patternType: 'glob',
              ruleType: 'exclude',
              description: '默认排除所有文件',
            },
            {
              pattern: 'X6Game/ScreenShot/**',
              patternType: 'glob',
              ruleType: 'include',
              description: '包含游戏截图目录',
            },
            {
              pattern: 'X6Game/Saved/GamePlayPhotos/*/NikkiPhotos_HighQuality/**',
              patternType: 'glob',
              ruleType: 'include',
              description: '包含高质量照片目录',
            },
          ],
        }
      }

      const result = await call(step.method, params, 60000) // 60秒超时
      const duration = Date.now() - startTime

      addLog({
        timestamp: Date.now(),
        step: step.method,
        status: 'success',
        message: `${step.description} - 完成`,
        data: result,
        duration,
      })

      return { success: true, data: result }
    } catch (error) {
      const duration = Date.now() - startTime
      const errorMessage = error instanceof Error ? error.message : String(error)

      addLog({
        timestamp: Date.now(),
        step: step.method,
        status: 'error',
        message: `${step.description} - 失败`,
        error: errorMessage,
        duration,
      })

      return { success: false, error: errorMessage }
    }
  }

  // 执行测试场景
  const executeScenario = async (scenarioId: string) => {
    const scenario = scenarios.value.find((s) => s.id === scenarioId)
    if (!scenario) {
      throw new Error(`Scenario not found: ${scenarioId}`)
    }

    if (executing.value) {
      throw new Error('Already executing a scenario')
    }

    executing.value = true
    const startTime = Date.now()

    // 初始化执行结果
    currentExecution.value = {
      scenarioId,
      startTime,
      status: 'running',
      logs: [],
    }

    try {
      const context: Record<string, unknown> = {}

      // 依次执行每个步骤
      for (const step of scenario.steps) {
        const result = await executeStep(step, context)

        if (!result.success) {
          throw new Error(`Step failed: ${step.method}`)
        }

        // 保存步骤结果到上下文
        if (step.method === 'plugins.infinityNikki.getGameDirectory') {
          // 从返回的对象中提取 gameDir 字符串
          context.gameDir = (result.data as any)?.gameDir || result.data
        }

        // 保存最终结果
        currentExecution.value.finalResult = result.data
      }

      const endTime = Date.now()
      currentExecution.value.endTime = endTime
      currentExecution.value.duration = endTime - startTime
      currentExecution.value.status = 'success'

      // 添加到历史记录
      executionHistory.value.unshift({ ...currentExecution.value })

      // 限制历史记录数量
      if (executionHistory.value.length > 20) {
        executionHistory.value = executionHistory.value.slice(0, 20)
      }
    } catch (error) {
      const endTime = Date.now()
      currentExecution.value.endTime = endTime
      currentExecution.value.duration = endTime - startTime
      currentExecution.value.status = 'error'

      // 添加到历史记录
      executionHistory.value.unshift({ ...currentExecution.value })
    } finally {
      executing.value = false
    }
  }

  // 清空当前执行结果
  const clearCurrentExecution = () => {
    currentExecution.value = null
  }

  // 清空历史记录
  const clearHistory = () => {
    executionHistory.value = []
  }

  // 根据分类获取场景
  const getScenariosByCategory = (category: ScenarioCategory) => {
    return scenarios.value.filter((s) => s.category === category)
  }

  return {
    executing,
    currentExecution,
    executionHistory,
    scenarios,
    executeScenario,
    clearCurrentExecution,
    clearHistory,
    getScenariosByCategory,
  }
}
