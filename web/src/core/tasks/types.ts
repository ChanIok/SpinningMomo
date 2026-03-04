export type TaskStatus = 'queued' | 'running' | 'succeeded' | 'failed' | 'cancelled'

export interface TaskProgress {
  stage: string
  current: number
  total: number
  percent?: number
  message?: string
}

export interface TaskSnapshot {
  taskId: string
  type: string
  status: TaskStatus
  createdAt: number
  startedAt?: number
  finishedAt?: number
  progress?: TaskProgress
  errorMessage?: string
  context?: string
}
