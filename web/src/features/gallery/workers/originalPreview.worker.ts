/// <reference lib="webworker" />

interface GenerateOriginalPreviewMessage {
  type: 'generate'
  id: number
  url: string
  targetShortEdge: number
  sourceWidth?: number
  sourceHeight?: number
  quality: number
}

interface CancelOriginalPreviewMessage {
  type: 'cancel'
  id: number
}

type OriginalPreviewWorkerMessage = GenerateOriginalPreviewMessage | CancelOriginalPreviewMessage

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

const activeFetchControllers = new Map<number, AbortController>()
const canceledRequestIds = new Set<number>()

// 从原图生成短边匹配卡片显示需求的完整比例预览图。
async function generateOriginalPreview(message: GenerateOriginalPreviewMessage) {
  const fetchController = new AbortController()
  activeFetchControllers.set(message.id, fetchController)

  try {
    // 先通过 fetch 读取虚拟主机映射下的原图文件。
    const response = await fetch(message.url, { signal: fetchController.signal })
    if (!response.ok) {
      throw new Error(`Failed to fetch image: ${response.status}`)
    }

    assertRequestActive(message.id)

    const sourceBlob = await response.blob()
    assertRequestActive(message.id)

    // 尽量在 createImageBitmap 阶段就按短边降采样，避免完整原图进入 canvas。
    const bitmap = await createShortEdgeImageBitmap(sourceBlob, message)
    try {
      assertRequestActive(message.id)

      const outputSize = calculateShortEdgeSize(
        bitmap.width,
        bitmap.height,
        message.targetShortEdge
      )
      const canvas = new OffscreenCanvas(outputSize.width, outputSize.height)
      const context = canvas.getContext('2d')
      if (!context) {
        throw new Error('Failed to create 2D canvas context')
      }

      // 输出完整比例图，让缩略图层和原图预览层都交给 CSS object-cover 投影。
      context.drawImage(bitmap, 0, 0, outputSize.width, outputSize.height)

      assertRequestActive(message.id)

      const previewBlob = await canvas.convertToBlob({
        type: 'image/webp',
        quality: message.quality,
      })

      assertRequestActive(message.id)
      postResponse({ id: message.id, ok: true, blob: previewBlob })
    } finally {
      // ImageBitmap 持有图形资源，完成后明确释放。
      bitmap.close()
    }
  } catch (error) {
    // 取消请求也要回包，让主线程释放对应的并发槽。
    postResponse({
      id: message.id,
      ok: false,
      error: error instanceof Error ? error.message : String(error),
    })
  } finally {
    activeFetchControllers.delete(message.id)
    canceledRequestIds.delete(message.id)
  }
}

// 创建按目标短边缩放后的 ImageBitmap。
async function createShortEdgeImageBitmap(
  sourceBlob: Blob,
  message: GenerateOriginalPreviewMessage
): Promise<ImageBitmap> {
  if (
    message.sourceWidth &&
    message.sourceHeight &&
    message.sourceWidth > 0 &&
    message.sourceHeight > 0
  ) {
    const targetSize = calculateShortEdgeSize(
      message.sourceWidth,
      message.sourceHeight,
      message.targetShortEdge
    )

    if (targetSize.width === message.sourceWidth && targetSize.height === message.sourceHeight) {
      return createImageBitmap(sourceBlob, { imageOrientation: 'from-image' })
    }

    return createImageBitmap(sourceBlob, {
      imageOrientation: 'from-image',
      resizeWidth: targetSize.width,
      resizeHeight: targetSize.height,
      resizeQuality: 'high',
    })
  }

  // 元数据缺失时回退到浏览器默认解码，再由 canvas 按短边缩放。
  return createImageBitmap(sourceBlob, { imageOrientation: 'from-image' })
}

// 按后端缩略图同款规则计算短边缩放尺寸。
function calculateShortEdgeSize(
  sourceWidth: number,
  sourceHeight: number,
  targetShortEdge: number
) {
  const normalizedTargetShortEdge = Math.max(1, Math.round(targetShortEdge))
  const sourceShortEdge = Math.min(sourceWidth, sourceHeight)

  if (sourceShortEdge <= normalizedTargetShortEdge) {
    return {
      width: sourceWidth,
      height: sourceHeight,
    }
  }

  const scale = normalizedTargetShortEdge / sourceShortEdge
  return {
    width: Math.max(1, Math.floor(sourceWidth * scale)),
    height: Math.max(1, Math.floor(sourceHeight * scale)),
  }
}

// 标记一个请求取消，并尽量中止仍在进行的 fetch。
function cancelOriginalPreview(id: number) {
  canceledRequestIds.add(id)
  activeFetchControllers.get(id)?.abort()
}

// 已取消请求不再继续 Worker 后续阶段。
function assertRequestActive(id: number) {
  if (isRequestCanceled(id)) {
    throw new DOMException('Original preview request was canceled', 'AbortError')
  }
}

function isRequestCanceled(id: number): boolean {
  return canceledRequestIds.has(id)
}

function postResponse(response: OriginalPreviewWorkerResponse) {
  self.postMessage(response)
}

self.addEventListener('message', (event: MessageEvent<OriginalPreviewWorkerMessage>) => {
  const message = event.data

  if (message.type === 'cancel') {
    cancelOriginalPreview(message.id)
    return
  }

  void generateOriginalPreview(message)
})
