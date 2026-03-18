import { computed, ref } from 'vue'
import { defineStore } from 'pinia'
import { call, on, off } from '@/core/rpc'
import type { TaskSnapshot } from './types'

interface ClearFinishedTasksResult {
  clearedCount: number
}

function isTaskActiveStatus(status: string): boolean {
  return status === 'queued' || status === 'running'
}

function isTaskSnapshot(value: unknown): value is TaskSnapshot {
  if (typeof value !== 'object' || value === null) {
    return false
  }

  const candidate = value as Record<string, unknown>
  return (
    typeof candidate.taskId === 'string' &&
    typeof candidate.type === 'string' &&
    typeof candidate.status === 'string' &&
    typeof candidate.createdAt === 'number'
  )
}

function sortTasks(tasks: TaskSnapshot[]): TaskSnapshot[] {
  return [...tasks].sort((a, b) => b.createdAt - a.createdAt)
}

export const useTaskStore = defineStore('core-task-store', () => {
  const tasks = ref<TaskSnapshot[]>([])
  const isInitialized = ref(false)
  const error = ref<string | null>(null)

  let taskUpdatedHandler: ((params: unknown) => void) | null = null

  function upsertTask(snapshot: TaskSnapshot): void {
    const index = tasks.value.findIndex((item) => item.taskId === snapshot.taskId)
    if (index >= 0) {
      tasks.value[index] = snapshot
    } else {
      tasks.value.push(snapshot)
    }

    tasks.value = sortTasks(tasks.value).slice(0, 50)
  }

  async function initialize(): Promise<void> {
    if (isInitialized.value) {
      return
    }

    try {
      const remoteTasks = await call<TaskSnapshot[]>('task.list', {})
      tasks.value = Array.isArray(remoteTasks)
        ? sortTasks(remoteTasks.filter((item) => isTaskSnapshot(item))).slice(0, 50)
        : []
      error.value = null
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
      tasks.value = []
    }

    taskUpdatedHandler = (params: unknown) => {
      if (!isTaskSnapshot(params)) {
        return
      }
      upsertTask(params)
    }

    on('task.updated', taskUpdatedHandler)
    isInitialized.value = true
  }

  function dispose(): void {
    if (taskUpdatedHandler) {
      off('task.updated', taskUpdatedHandler)
      taskUpdatedHandler = null
    }
    isInitialized.value = false
  }

  const activeTasks = computed(() => tasks.value.filter((item) => isTaskActiveStatus(item.status)))

  async function clearFinished(): Promise<number> {
    const result = await call<ClearFinishedTasksResult>('task.clearFinished', {})
    tasks.value = tasks.value.filter((item) => isTaskActiveStatus(item.status))
    return result.clearedCount
  }

  return {
    tasks,
    activeTasks,
    isInitialized,
    error,
    initialize,
    clearFinished,
    dispose,
  }
})
