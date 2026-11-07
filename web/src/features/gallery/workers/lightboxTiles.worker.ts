/// <reference lib="webworker" />

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

type LightboxTileWorkerMessage = StartTileStreamMessage | AckTileMessage | CancelTileStreamMessage

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

let activeId = 0
let activeController: AbortController | null = null
let pendingAck: (() => void) | null = null

// 判断消息是否仍属于当前流，旧流的异步结果一律丢弃。
function isActive(id: number): boolean {
  return id === activeId
}

// 暂停切片生成，等待主线程完成当前 tile 的 GPU 上传。
function waitForTileAck(): Promise<void> {
  return new Promise((resolve) => {
    pendingAck = () => resolve()
  })
}

// 下载原图并在 Worker 内完成带 EXIF 方向的 bitmap 解码。
async function loadSourceBitmap(url: string, signal: AbortSignal): Promise<ImageBitmap> {
  const response = await fetch(url, { signal })
  if (!response.ok) {
    throw new Error(`Failed to fetch image: ${response.status}`)
  }

  const blob = await response.blob()
  return createImageBitmap(blob, { imageOrientation: 'from-image' })
}

// 按实际 bitmap 尺寸逐块生成 ImageBitmap，并用 ack 控制传输背压。
async function streamTiles(message: StartTileStreamMessage) {
  const controller = new AbortController()
  activeController = controller
  activeId = message.id

  try {
    if (!isActive(message.id)) return
    const sourceBitmap = await loadSourceBitmap(message.url, controller.signal)
    try {
      // 解码后的尺寸是切片坐标的唯一来源，避免元数据与 EXIF 方向不一致。
      const width = sourceBitmap.width
      const height = sourceBitmap.height
      // 先回报真实尺寸，让主线程决定是否接受 tiled 路径。
      self.postMessage({
        type: 'meta',
        id: message.id,
        width,
        height,
      } satisfies LightboxTileWorkerResponse)

      for (let y = 0; y < height; y += message.tileSize) {
        for (let x = 0; x < width; x += message.tileSize) {
          if (!isActive(message.id)) return

          const tileWidth = Math.min(message.tileSize, width - x)
          const tileHeight = Math.min(message.tileSize, height - y)
          // 只生成当前 tile，发送后等待主线程 ack 再继续。
          const tileBitmap = await createImageBitmap(sourceBitmap, x, y, tileWidth, tileHeight)

          if (!isActive(message.id)) {
            tileBitmap.close()
            return
          }

          self.postMessage(
            {
              type: 'tile',
              id: message.id,
              x,
              y,
              width: tileWidth,
              height: tileHeight,
              bitmap: tileBitmap,
            } satisfies LightboxTileWorkerResponse,
            [tileBitmap]
          )

          await waitForTileAck()
        }
      }

      if (isActive(message.id)) {
        self.postMessage({ type: 'complete', id: message.id } satisfies LightboxTileWorkerResponse)
      }
    } finally {
      // 整图 bitmap 只服务当前上传流；流结束后立即释放 Worker 内存。
      sourceBitmap.close()
    }
  } catch (error) {
    if (isActive(message.id)) {
      self.postMessage({
        type: 'error',
        id: message.id,
        error: error instanceof Error ? error.message : String(error),
      } satisfies LightboxTileWorkerResponse)
    }
  } finally {
    if (activeController === controller) activeController = null
    if (isActive(message.id)) {
      activeId = 0
      pendingAck = null
    }
  }
}

// 中止当前 fetch 和 ack 等待，让正在运行的流尽快退出。
function cancel(id: number) {
  if (!isActive(id)) return
  activeId = 0
  activeController?.abort()
  activeController = null
  pendingAck?.()
  pendingAck = null
}

self.addEventListener('message', (event: MessageEvent<LightboxTileWorkerMessage>) => {
  const message = event.data

  if (message.type === 'cancel') {
    // 取消请求只影响对应的活动流。
    cancel(message.id)
    return
  }

  if (message.type === 'ack') {
    // 收到主线程 ack 后继续生成下一块。
    if (isActive(message.id)) {
      pendingAck?.()
      pendingAck = null
    }
    return
  }

  // 新的 start 会替换旧流，避免两个原图同时占用 Worker。
  cancel(activeId)
  void streamTiles(message)
})
