<script setup lang="ts">
import { computed } from 'vue'
import { PlayIcon, Trash2Icon, ClockIcon } from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import JsonResponseViewer from '../components/JsonResponseViewer.vue'
import { useIntegrationTest } from '../composables/useIntegrationTest'
import type { TestScenario, ExecutionLog } from '../composables/useIntegrationTest'

const {
  executing,
  currentExecution,
  executionHistory,
  scenarios,
  executeScenario,
  clearCurrentExecution,
  clearHistory,
  getScenariosByCategory,
} = useIntegrationTest()

// 获取分类后的场景
const galleryScenarios = computed(() => getScenariosByCategory('gallery'))
const databaseScenarios = computed(() => getScenariosByCategory('database'))
const systemScenarios = computed(() => getScenariosByCategory('system'))

// 执行场景
const handleExecuteScenario = async (scenario: TestScenario) => {
  if (executing.value) return

  try {
    await executeScenario(scenario.id)
  } catch (error) {
    console.error('Failed to execute scenario:', error)
  }
}

// 格式化时间
const formatTime = (timestamp: number) => {
  return new Date(timestamp).toLocaleTimeString('zh-CN')
}

// 格式化持续时间
const formatDuration = (duration?: number) => {
  if (!duration) return '-'
  if (duration < 1000) return `${duration}ms`
  return `${(duration / 1000).toFixed(2)}s`
}

// 获取日志状态的颜色类
const getLogStatusClass = (status: ExecutionLog['status']) => {
  switch (status) {
    case 'running':
      return 'text-blue-600 dark:text-blue-400'
    case 'success':
      return 'text-green-600 dark:text-green-400'
    case 'error':
      return 'text-red-600 dark:text-red-400'
    default:
      return 'text-gray-600 dark:text-gray-400'
  }
}

// 获取日志状态的图标
const getLogStatusIcon = (status: ExecutionLog['status']) => {
  switch (status) {
    case 'running':
      return '⏳'
    case 'success':
      return '✅'
    case 'error':
      return '❌'
    default:
      return '⚪'
  }
}
</script>

<template>
  <div class="flex h-full flex-col">
    <!-- 主要内容 -->
    <div class="flex-1 overflow-y-auto p-4">
      <div class="mx-auto max-w-6xl space-y-6">
        <!-- 图库管理场景 -->
        <section v-if="galleryScenarios.length > 0">
          <h2 class="mb-3 text-lg font-semibold">📸 图库管理</h2>
          <div class="grid gap-3 md:grid-cols-2 lg:grid-cols-3">
            <div
              v-for="scenario in galleryScenarios"
              :key="scenario.id"
              class="rounded-lg border bg-card p-4 shadow-sm transition-shadow hover:shadow-md"
            >
              <div class="mb-2 flex items-start justify-between">
                <div class="flex items-center gap-2">
                  <span class="text-2xl">{{ scenario.icon }}</span>
                  <h3 class="font-medium">{{ scenario.name }}</h3>
                </div>
                <Badge v-if="scenario.dangerous" variant="destructive" class="text-xs">
                  危险
                </Badge>
              </div>
              <p class="mb-4 text-sm text-muted-foreground">
                {{ scenario.description }}
              </p>
              <div class="mb-3 space-y-1">
                <p class="text-xs text-muted-foreground">执行步骤：</p>
                <ul class="space-y-1 text-xs text-muted-foreground">
                  <li
                    v-for="(step, index) in scenario.steps"
                    :key="index"
                    class="flex items-start gap-1"
                  >
                    <span class="mt-0.5">{{ index + 1 }}.</span>
                    <span>{{ step.description }}</span>
                  </li>
                </ul>
              </div>
              <Button
                @click="handleExecuteScenario(scenario)"
                :disabled="executing"
                class="w-full"
                size="sm"
              >
                <PlayIcon v-if="!executing" class="mr-2 h-3.5 w-3.5" />
                <div
                  v-else
                  class="mr-2 h-3.5 w-3.5 animate-spin rounded-full border-2 border-current border-t-transparent"
                />
                {{ executing ? '执行中...' : '执行' }}
              </Button>
            </div>
          </div>
        </section>

        <!-- 数据库管理场景 -->
        <section v-if="databaseScenarios.length > 0">
          <h2 class="mb-3 text-lg font-semibold">🗄️ 数据库管理</h2>
          <div class="grid gap-3 md:grid-cols-2 lg:grid-cols-3">
            <div
              v-for="scenario in databaseScenarios"
              :key="scenario.id"
              class="rounded-lg border bg-card p-4 shadow-sm transition-shadow hover:shadow-md"
            >
              <div class="mb-2 flex items-start justify-between">
                <div class="flex items-center gap-2">
                  <span class="text-2xl">{{ scenario.icon }}</span>
                  <h3 class="font-medium">{{ scenario.name }}</h3>
                </div>
                <Badge v-if="scenario.dangerous" variant="destructive" class="text-xs">
                  危险
                </Badge>
              </div>
              <p class="mb-4 text-sm text-muted-foreground">
                {{ scenario.description }}
              </p>
              <div class="mb-3 space-y-1">
                <p class="text-xs text-muted-foreground">执行步骤：</p>
                <ul class="space-y-1 text-xs text-muted-foreground">
                  <li
                    v-for="(step, index) in scenario.steps"
                    :key="index"
                    class="flex items-start gap-1"
                  >
                    <span class="mt-0.5">{{ index + 1 }}.</span>
                    <span>{{ step.description }}</span>
                  </li>
                </ul>
              </div>
              <Button
                @click="handleExecuteScenario(scenario)"
                :disabled="executing"
                class="w-full"
                size="sm"
                :variant="scenario.dangerous ? 'destructive' : 'default'"
              >
                <PlayIcon v-if="!executing" class="mr-2 h-3.5 w-3.5" />
                <div
                  v-else
                  class="mr-2 h-3.5 w-3.5 animate-spin rounded-full border-2 border-current border-t-transparent"
                />
                {{ executing ? '执行中...' : '执行' }}
              </Button>
            </div>
          </div>
        </section>

        <!-- 系统管理场景 -->
        <section v-if="systemScenarios.length > 0">
          <h2 class="mb-3 text-lg font-semibold">⚙️ 系统管理</h2>
          <div class="grid gap-3 md:grid-cols-2 lg:grid-cols-3">
            <div
              v-for="scenario in systemScenarios"
              :key="scenario.id"
              class="rounded-lg border bg-card p-4 shadow-sm transition-shadow hover:shadow-md"
            >
              <div class="mb-2 flex items-start justify-between">
                <div class="flex items-center gap-2">
                  <span class="text-2xl">{{ scenario.icon }}</span>
                  <h3 class="font-medium">{{ scenario.name }}</h3>
                </div>
                <Badge v-if="scenario.dangerous" variant="destructive" class="text-xs">
                  危险
                </Badge>
              </div>
              <p class="mb-4 text-sm text-muted-foreground">
                {{ scenario.description }}
              </p>
              <div class="mb-3 space-y-1">
                <p class="text-xs text-muted-foreground">执行步骤：</p>
                <ul class="space-y-1 text-xs text-muted-foreground">
                  <li
                    v-for="(step, index) in scenario.steps"
                    :key="index"
                    class="flex items-start gap-1"
                  >
                    <span class="mt-0.5">{{ index + 1 }}.</span>
                    <span>{{ step.description }}</span>
                  </li>
                </ul>
              </div>
              <Button
                @click="handleExecuteScenario(scenario)"
                :disabled="executing"
                class="w-full"
                size="sm"
                :variant="scenario.dangerous ? 'destructive' : 'default'"
              >
                <PlayIcon v-if="!executing" class="mr-2 h-3.5 w-3.5" />
                <div
                  v-else
                  class="mr-2 h-3.5 w-3.5 animate-spin rounded-full border-2 border-current border-t-transparent"
                />
                {{ executing ? '执行中...' : '执行' }}
              </Button>
            </div>
          </div>
        </section>

        <!-- 执行结果 -->
        <section v-if="currentExecution" class="rounded-lg border bg-card">
          <div class="border-b p-4">
            <div class="flex items-center justify-between">
              <h2 class="text-lg font-semibold">📊 执行结果</h2>
              <Button @click="clearCurrentExecution" variant="ghost" size="sm">
                <Trash2Icon class="mr-2 h-4 w-4" />
                清空
              </Button>
            </div>
          </div>

          <div class="p-4">
            <!-- 执行信息 -->
            <div class="mb-4 flex items-center gap-4 text-sm">
              <div class="flex items-center gap-2">
                <span class="text-muted-foreground">状态:</span>
                <Badge
                  :variant="
                    currentExecution.status === 'success'
                      ? 'default'
                      : currentExecution.status === 'error'
                        ? 'destructive'
                        : 'secondary'
                  "
                >
                  {{
                    currentExecution.status === 'running'
                      ? '执行中'
                      : currentExecution.status === 'success'
                        ? '成功'
                        : '失败'
                  }}
                </Badge>
              </div>
              <div class="flex items-center gap-2">
                <ClockIcon class="h-4 w-4 text-muted-foreground" />
                <span class="text-muted-foreground">耗时:</span>
                <span class="font-mono">{{ formatDuration(currentExecution.duration) }}</span>
              </div>
              <div class="flex items-center gap-2">
                <span class="text-muted-foreground">开始时间:</span>
                <span class="font-mono">{{ formatTime(currentExecution.startTime) }}</span>
              </div>
            </div>

            <!-- 执行日志 -->
            <div class="mb-4">
              <h3 class="mb-2 text-sm font-medium">执行日志</h3>
              <div class="rounded-md border bg-muted/50 p-3">
                <div class="space-y-2 font-mono text-xs">
                  <div
                    v-for="(log, index) in currentExecution.logs"
                    :key="index"
                    class="flex items-start gap-2"
                    :class="getLogStatusClass(log.status)"
                  >
                    <span class="flex-shrink-0">{{ getLogStatusIcon(log.status) }}</span>
                    <span class="flex-shrink-0 text-muted-foreground">
                      {{ formatTime(log.timestamp) }}
                    </span>
                    <span class="flex-1">{{ log.message }}</span>
                    <span v-if="log.duration" class="flex-shrink-0 text-muted-foreground">
                      ({{ formatDuration(log.duration) }})
                    </span>
                  </div>
                  <div v-if="currentExecution.logs.length === 0" class="text-muted-foreground">
                    暂无日志
                  </div>
                </div>
              </div>
            </div>

            <!-- 响应数据 -->
            <div>
              <h3 class="mb-2 text-sm font-medium">响应数据</h3>
              <JsonResponseViewer
                :response="{
                  success: currentExecution.status === 'success',
                  data: currentExecution.finalResult,
                  timestamp: currentExecution.startTime,
                  duration: currentExecution.duration || 0,
                }"
                :loading="currentExecution.status === 'running'"
              />
            </div>
          </div>
        </section>

        <!-- 执行历史 -->
        <section v-if="executionHistory.length > 0" class="rounded-lg border bg-card">
          <div class="border-b p-4">
            <div class="flex items-center justify-between">
              <h2 class="text-lg font-semibold">📜 执行历史</h2>
              <Button @click="clearHistory" variant="ghost" size="sm">
                <Trash2Icon class="mr-2 h-4 w-4" />
                清空
              </Button>
            </div>
          </div>

          <div class="divide-y">
            <div
              v-for="(execution, index) in executionHistory.slice(0, 5)"
              :key="index"
              class="p-4"
            >
              <div class="flex items-center justify-between">
                <div class="flex items-center gap-3">
                  <Badge
                    :variant="
                      execution.status === 'success'
                        ? 'default'
                        : execution.status === 'error'
                          ? 'destructive'
                          : 'secondary'
                    "
                  >
                    {{
                      execution.status === 'running'
                        ? '执行中'
                        : execution.status === 'success'
                          ? '成功'
                          : '失败'
                    }}
                  </Badge>
                  <span class="text-sm font-medium">
                    {{
                      scenarios.find((s) => s.id === execution.scenarioId)?.name ||
                      execution.scenarioId
                    }}
                  </span>
                </div>
                <div class="flex items-center gap-4 text-sm text-muted-foreground">
                  <span class="font-mono">{{ formatDuration(execution.duration) }}</span>
                  <span class="font-mono">{{ formatTime(execution.startTime) }}</span>
                </div>
              </div>
            </div>
          </div>
        </section>
      </div>
    </div>
  </div>
</template>
