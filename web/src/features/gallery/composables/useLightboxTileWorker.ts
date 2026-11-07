import { onScopeDispose } from 'vue'

export interface LightboxTile {
  x: number
  y: number
  width: number
  height: number
  bitmap: ImageBitmap
}

export interface LightboxTileSourceSize {
  width: number
  height: number
}

interface StartTileStreamMessage {
  type: 'start'
  id: number
  url: string
  tileSize: number
}

interface AckTileMessage {
  type: 'ack'
  id: number
}

interface CancelTileStreamMessage {
  type: 'cancel'
  id: number
}

type LightboxTileWorkerResponse =
  | {
      type: 'meta'
      id: number
      width: number
      height: number
    }
  | {
      type: 'tile'
      id: number
      x: number
      y: number
      width: number
      height: number
      bitmap: ImageBitmap
    }
  | {
      type: 'complete'
      id: number
    }
  | {
      type: 'error'
      id: number
      error: string
    }

interface StreamRequest {
  url: string
  tileSize: number
  onMeta: (size: LightboxTileSourceSize) => void
  onTile: (tile: LightboxTile) => Promise<void>
}

interface ActiveRequest {
  id: number
  resolve: (success: boolean | null) => void
  reject: (error: unknown) => void
  onMeta: (size: LightboxTileSourceSize) => void
  onTile: (tile: LightboxTile) => Promise<void>
}

/**
 * 一个 renderer 一个 Worker；取消只影响当前 renderer，不会误伤其它调用者。
 * Worker 每次只发送一个 tile，主线程上传完成后才 ack，保证传输背压稳定。
 */
export function useLightboxTileWorker() {
  let worker: Worker | null = null
  let nextRequestId = 1
  let activeRequest: ActiveRequest | null = null

  // 创建当前 renderer 专属的模块 Worker，并注册消息和异常入口。
  function ensureWorker(): Worker {
    if (worker) return worker

    worker = new Worker(new URL('../workers/lightboxTiles.worker.ts', import.meta.url), {
      type: 'module',
    })
    worker.addEventListener('message', handleWorkerMessage)
    worker.addEventListener('error', handleWorkerFailure)
    worker.addEventListener('messageerror', handleWorkerFailure)
    return worker
  }

  // 取消当前 tile 流并让调用方把它视为过期请求。
  function cancelActiveRequest() {
    const request = activeRequest
    if (!request) return

    worker?.postMessage({ type: 'cancel', id: request.id } satisfies CancelTileStreamMessage)
    activeRequest = null
    request.resolve(null)
  }

  // 让当前流进入失败状态，避免旧消息继续触碰新的请求。
  function rejectActiveRequest(error: unknown) {
    const request = activeRequest
    if (!request) return

    activeRequest = null
    request.reject(error)
  }

  // 分发 Worker 的尺寸、tile、完成和错误消息，并对每个 tile 回传 ack。
  function handleWorkerMessage(event: MessageEvent<LightboxTileWorkerResponse>) {
    const response = event.data
    const request = activeRequest
    if (!request || response.id !== request.id) {
      if (response.type === 'tile') response.bitmap.close()
      return
    }

    try {
      if (response.type === 'meta') {
        // 先把真实 bitmap 尺寸交给 renderer，后续切片坐标以它为准。
        request.onMeta({ width: response.width, height: response.height })
        return
      }

      if (response.type === 'complete') {
        // Worker 已发送完所有 tile，结束当前流。
        activeRequest = null
        request.resolve(true)
        return
      }

      if (response.type === 'error') {
        // Worker 无法获取或解码原图，交给 renderer 选择普通图片兜底。
        rejectActiveRequest(new Error(response.error))
        return
      }

      void request
        .onTile({
          x: response.x,
          y: response.y,
          width: response.width,
          height: response.height,
          bitmap: response.bitmap,
        })
        .then(() => {
          // 主线程完成 GPU 上传后才允许 Worker 继续生成下一块。
          if (activeRequest?.id !== response.id || !worker) return
          worker.postMessage({ type: 'ack', id: response.id } satisfies AckTileMessage)
        })
        .catch((error: unknown) => {
          // tile 上传失败时同时中止 Worker，避免它继续等待无效 ack。
          if (activeRequest?.id !== response.id) return
          worker?.postMessage({ type: 'cancel', id: response.id } satisfies CancelTileStreamMessage)
          rejectActiveRequest(error)
        })
    } catch (error) {
      // 回调本身失败也要取消 Worker，保证流不会卡在当前 tile。
      worker?.postMessage({ type: 'cancel', id: response.id } satisfies CancelTileStreamMessage)
      rejectActiveRequest(error)
    }
  }

  // Worker 崩溃时终止旧实例，并让当前调用立即进入失败路径。
  function handleWorkerFailure(event: Event | ErrorEvent) {
    const error =
      event instanceof ErrorEvent && event.error
        ? event.error
        : new Error(event instanceof ErrorEvent ? event.message : 'Lightbox tile worker failed')

    worker?.terminate()
    worker = null
    rejectActiveRequest(error)
  }

  // 启动一条新的 tile 流；同一 renderer 同时只允许一个流。
  function stream(request: StreamRequest): Promise<boolean | null> {
    // 新流优先，先取消可能仍在预加载的旧流。
    cancelActiveRequest()

    const id = nextRequestId++
    const promise = new Promise<boolean | null>((resolve, reject) => {
      activeRequest = {
        id,
        resolve,
        reject,
        onMeta: request.onMeta,
        onTile: request.onTile,
      }
    })

    try {
      // 传递 URL 和 tile 尺寸，真实图片尺寸由 Worker 解码后回报。
      ensureWorker().postMessage({
        type: 'start',
        id,
        url: request.url,
        tileSize: request.tileSize,
      } satisfies StartTileStreamMessage)
    } catch (error) {
      // postMessage 失败时立即拒绝 Promise，不留下悬挂请求。
      rejectActiveRequest(error)
    }

    return promise
  }

  // 关闭 Worker 并结束当前 renderer 的所有后台任务。
  function dispose() {
    cancelActiveRequest()
    worker?.terminate()
    worker = null
  }

  onScopeDispose(dispose)

  return {
    stream,
    cancel: cancelActiveRequest,
    dispose,
  }
}
