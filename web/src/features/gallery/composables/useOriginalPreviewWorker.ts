const MAX_CONCURRENT_ORIGINAL_PREVIEW_JOBS = 2
const ORIGINAL_PREVIEW_OUTPUT_QUALITY = 1

interface GenerateOriginalPreviewWorkerMessage {
  type: 'generate'
  id: number
  url: string
  targetShortEdge: number
  sourceWidth?: number
  sourceHeight?: number
  quality: number
}

interface CancelOriginalPreviewWorkerMessage {
  type: 'cancel'
  id: number
}

type OriginalPreviewWorkerMessage =
  GenerateOriginalPreviewWorkerMessage | CancelOriginalPreviewWorkerMessage

type OriginalPreviewWorkerResponse =
  | {
      id: number
      ok: true
      blob: Blob
    }
  | {
      id: number
      ok: false
      error: string
    }

export interface OriginalPreviewRequest {
  url: string
  targetShortEdge: number
  sourceWidth?: number | null
  sourceHeight?: number | null
  signal?: AbortSignal
}

interface QueuedOriginalPreviewRequest {
  id: number
  request: OriginalPreviewRequest
  resolve: (blob: Blob) => void
  reject: (error: unknown) => void
  abortHandler: () => void
}

interface ActiveOriginalPreviewRequest {
  resolve: (blob: Blob) => void
  reject: (error: unknown) => void
  abortHandler: () => void
  signal?: AbortSignal
  settled: boolean
}

let worker: Worker | null = null
let nextRequestId = 1
let activePreviewJobs = 0
const queuedRequests: QueuedOriginalPreviewRequest[] = []
const activeRequests = new Map<number, ActiveOriginalPreviewRequest>()

// 提供共享 Worker 请求入口，把原图压成短边匹配卡片显示的临时预览 Blob。
export function useOriginalPreviewWorker() {
  return {
    generateOriginalPreview,
  }
}

// 生成一张按目标短边等比缩放后的原图预览。
function generateOriginalPreview(request: OriginalPreviewRequest): Promise<Blob> {
  if (request.signal?.aborted) {
    return Promise.reject(createAbortError())
  }

  const normalizedRequest = normalizeOriginalPreviewRequest(request)
  const id = nextRequestId++

  return new Promise((resolve, reject) => {
    const queuedRequest: QueuedOriginalPreviewRequest = {
      id,
      request: normalizedRequest,
      resolve,
      reject,
      abortHandler: () => {
        cancelQueuedRequest(id)
      },
    }

    // 排队阶段可以直接移除任务，避免未开始的请求进入 Worker。
    normalizedRequest.signal?.addEventListener('abort', queuedRequest.abortHandler, { once: true })
    queuedRequests.push(queuedRequest)
    startNextPreviewJob()
  })
}

// 规范化目标短边，避免 Worker 收到 0 或小数尺寸。
function normalizeOriginalPreviewRequest(request: OriginalPreviewRequest): OriginalPreviewRequest {
  return {
    ...request,
    targetShortEdge: Math.max(1, Math.round(request.targetShortEdge)),
    sourceWidth: request.sourceWidth && request.sourceWidth > 0 ? request.sourceWidth : undefined,
    sourceHeight:
      request.sourceHeight && request.sourceHeight > 0 ? request.sourceHeight : undefined,
  }
}

// 启动队列中的下一项，并保持全局并发上限。
function startNextPreviewJob() {
  while (activePreviewJobs < MAX_CONCURRENT_ORIGINAL_PREVIEW_JOBS && queuedRequests.length > 0) {
    const queuedRequest = queuedRequests.shift()!
    queuedRequest.request.signal?.removeEventListener('abort', queuedRequest.abortHandler)

    if (queuedRequest.request.signal?.aborted) {
      queuedRequest.reject(createAbortError())
      continue
    }

    startPreviewJob(queuedRequest)
  }
}

// 把一个请求发送到 Worker，并接管 active 阶段的取消和完成回调。
function startPreviewJob(queuedRequest: QueuedOriginalPreviewRequest) {
  const previewWorker = ensureOriginalPreviewWorker()
  activePreviewJobs += 1

  const activeRequest: ActiveOriginalPreviewRequest = {
    resolve: queuedRequest.resolve,
    reject: queuedRequest.reject,
    signal: queuedRequest.request.signal,
    settled: false,
    abortHandler: () => {
      // Worker 内部会尽量中止 fetch；若已进入解码阶段，则等待结果返回后再释放槽位。
      previewWorker.postMessage({
        type: 'cancel',
        id: queuedRequest.id,
      } satisfies OriginalPreviewWorkerMessage)
      settleActiveRequest(queuedRequest.id, createAbortError())
    },
  }

  activeRequest.signal?.addEventListener('abort', activeRequest.abortHandler, { once: true })
  activeRequests.set(queuedRequest.id, activeRequest)

  previewWorker.postMessage({
    type: 'generate',
    id: queuedRequest.id,
    url: queuedRequest.request.url,
    targetShortEdge: queuedRequest.request.targetShortEdge,
    sourceWidth: queuedRequest.request.sourceWidth ?? undefined,
    sourceHeight: queuedRequest.request.sourceHeight ?? undefined,
    quality: ORIGINAL_PREVIEW_OUTPUT_QUALITY,
  } satisfies OriginalPreviewWorkerMessage)
}

// 获取共享 Worker 实例，并绑定全局响应处理。
function ensureOriginalPreviewWorker(): Worker {
  if (worker) {
    return worker
  }

  worker = new Worker(new URL('../workers/originalPreview.worker.ts', import.meta.url), {
    type: 'module',
  })
  worker.addEventListener('message', handleWorkerMessage)
  worker.addEventListener('error', handleWorkerFailure)
  worker.addEventListener('messageerror', handleWorkerFailure)
  return worker
}

// 处理 Worker 生成结果，并释放对应并发槽。
function handleWorkerMessage(event: MessageEvent<OriginalPreviewWorkerResponse>) {
  const response = event.data
  const activeRequest = activeRequests.get(response.id)
  if (!activeRequest) {
    return
  }

  releaseActiveRequest(response.id)

  if (activeRequest.settled) {
    return
  }

  activeRequest.settled = true
  if (response.ok) {
    activeRequest.resolve(response.blob)
    return
  }

  activeRequest.reject(new Error(response.error))
}

// Worker 失败时清空所有请求，避免卡片一直等待。
function handleWorkerFailure(event: Event | ErrorEvent) {
  const error =
    event instanceof ErrorEvent && event.error
      ? event.error
      : new Error(event instanceof ErrorEvent ? event.message : 'Original preview worker failed')

  worker?.terminate()
  worker = null

  queuedRequests.splice(0).forEach((request) => {
    request.request.signal?.removeEventListener('abort', request.abortHandler)
    request.reject(error)
  })

  const activeEntries = Array.from(activeRequests.entries())
  activeRequests.clear()
  activePreviewJobs = 0

  activeEntries.forEach(([, request]) => {
    request.signal?.removeEventListener('abort', request.abortHandler)
    if (!request.settled) {
      request.settled = true
      request.reject(error)
    }
  })
}

// 取消仍在队列中的请求。
function cancelQueuedRequest(id: number) {
  const index = queuedRequests.findIndex((request) => request.id === id)
  if (index === -1) {
    return
  }

  const [request] = queuedRequests.splice(index, 1)
  request?.request.signal?.removeEventListener('abort', request.abortHandler)
  request?.reject(createAbortError())
}

// 让 active 请求对调用方立即结束，但并发槽等 Worker 回包后释放。
function settleActiveRequest(id: number, error: unknown) {
  const activeRequest = activeRequests.get(id)
  if (!activeRequest || activeRequest.settled) {
    return
  }

  activeRequest.settled = true
  activeRequest.signal?.removeEventListener('abort', activeRequest.abortHandler)
  activeRequest.reject(error)
}

// 释放 active 请求占用的并发槽，并推进队列。
function releaseActiveRequest(id: number) {
  const activeRequest = activeRequests.get(id)
  if (!activeRequest) {
    return
  }

  activeRequest.signal?.removeEventListener('abort', activeRequest.abortHandler)
  activeRequests.delete(id)
  activePreviewJobs = Math.max(0, activePreviewJobs - 1)
  startNextPreviewJob()
}

function createAbortError(): DOMException {
  return new DOMException('Original preview request was canceled', 'AbortError')
}
