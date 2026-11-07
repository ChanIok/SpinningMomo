import { onMounted, onUnmounted, ref, watch, type Ref } from 'vue'
import { LIGHTBOX_TILE_SIZE } from '../constants'
import { useLightboxTileWorker, type LightboxTile } from './useLightboxTileWorker'

export interface LightboxTileRequest {
  url: string
  width: number
  height: number
}

export type LightboxTileShowResult =
  { kind: 'ready' } | { kind: 'fallback'; reason: string } | { kind: 'stale' }

interface TileTexture {
  x: number
  y: number
  width: number
  height: number
  texture: WebGLTexture
}

interface TileCacheEntry {
  url: string
  width: number
  height: number
  tiles: TileTexture[]
}

interface ActiveJob {
  generation: number
  request: LightboxTileRequest
  entry: TileCacheEntry
  prefetch: boolean
  cancelPendingUpload: (() => void) | null
}

interface UseLightboxTiledRendererOptions {
  canvasRef: Ref<HTMLCanvasElement | null>
  viewportRef: Ref<HTMLElement | null>
  stageRef: Ref<HTMLElement | null>
  displayScale: Readonly<Ref<number>>
  rotationDegrees: Readonly<Ref<number>>
  onRuntimeFailure?: () => void
}

const VERTEX_SHADER_SOURCE = `#version 300 es
in vec2 a_position;
in vec2 a_texcoord;
out vec2 v_texcoord;

void main() {
  gl_Position = vec4(a_position, 0.0, 1.0);
  v_texcoord = a_texcoord;
}
`

const FRAGMENT_SHADER_SOURCE = `#version 300 es
precision highp float;

uniform sampler2D u_texture;
in vec2 v_texcoord;
out vec4 outColor;

void main() {
  outColor = texture(u_texture, v_texcoord);
}
`

/**
 * 将一张超大原图按 tile 上传到 GPU。
 *
 * 这里刻意不做金字塔和可见区域裁剪：目标只是把大纹理上传拆到多个帧，
 * 同时让当前图和相邻图复用已经完成的 GPU texture。
 */
export function useLightboxTiledRenderer(options: UseLightboxTiledRendererOptions) {
  const { canvasRef, viewportRef, stageRef, displayScale, rotationDegrees, onRuntimeFailure } =
    options
  const tileWorker = useLightboxTileWorker()

  const available = ref(true)
  let gl: WebGL2RenderingContext | null = null
  let program: WebGLProgram | null = null
  let positionBuffer: WebGLBuffer | null = null
  let texcoordBuffer: WebGLBuffer | null = null
  let positionLocation = -1
  let texcoordLocation = -1
  let textureLocation: WebGLUniformLocation | null = null
  let renderFrame: number | null = null
  let renderWaiters: Array<{
    entry: TileCacheEntry
    resolve: (painted: boolean) => void
  }> = []
  let resizeObserver: ResizeObserver | null = null
  let generation = 0
  let activeJob: ActiveJob | null = null
  let visibleEntry: TileCacheEntry | null = null
  let prefetchQueue: LightboxTileRequest[] = []
  let retainedUrls = new Set<string>()
  const cache = new Map<string, TileCacheEntry>()

  // 编译单个 WebGL shader，失败时返回 null 让调用方走普通图片兜底。
  function compileShader(type: GLenum, source: string): WebGLShader | null {
    if (!gl) return null
    const shader = gl.createShader(type)
    if (!shader) return null
    gl.shaderSource(shader, source)
    gl.compileShader(shader)
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      gl.deleteShader(shader)
      return null
    }
    return shader
  }

  // 删除一个 tile 集合持有的全部 GPU texture。
  function releaseEntry(entry: TileCacheEntry) {
    for (const tile of entry.tiles) gl?.deleteTexture(tile.texture)
    entry.tiles = []
  }

  // 释放所有已完成的 tile 缓存。
  function releaseCache() {
    for (const entry of cache.values()) releaseEntry(entry)
    cache.clear()
  }

  // 销毁 WebGL 资源并清空缓存，供 context 丢失或卸载时调用。
  function releaseContext() {
    releaseCache()
    if (!gl) return
    if (positionBuffer) gl.deleteBuffer(positionBuffer)
    if (texcoordBuffer) gl.deleteBuffer(texcoordBuffer)
    if (program) gl.deleteProgram(program)
    positionBuffer = null
    texcoordBuffer = null
    program = null
    textureLocation = null
    positionLocation = -1
    texcoordLocation = -1
    gl = null
  }

  // 初始化 WebGL2 程序、缓冲区和纹理采样状态。
  function initializeContext(): boolean {
    if (!available.value) return false
    if (gl?.isContextLost()) {
      // 丢失的 context 不能继续复用，清掉旧 GPU 资源等待下次初始化。
      available.value = false
      releaseContext()
      return false
    }
    if (gl) return Boolean(program && positionBuffer && texcoordBuffer && textureLocation)

    const canvas = canvasRef.value
    if (!canvas) return false
    gl = canvas.getContext('webgl2', {
      alpha: true,
      antialias: false,
      depth: false,
      premultipliedAlpha: false,
    })
    if (!gl) {
      // 当前环境没有 WebGL2，当前图片交给普通 img。
      available.value = false
      return false
    }

    const vertexShader = compileShader(gl.VERTEX_SHADER, VERTEX_SHADER_SOURCE)
    const fragmentShader = compileShader(gl.FRAGMENT_SHADER, FRAGMENT_SHADER_SOURCE)
    if (!vertexShader || !fragmentShader) {
      if (vertexShader) gl.deleteShader(vertexShader)
      if (fragmentShader) gl.deleteShader(fragmentShader)
      available.value = false
      releaseContext()
      return false
    }

    // 链接绘制程序，后续所有 tile 共用这一套 shader。
    program = gl.createProgram()
    if (!program) {
      gl.deleteShader(vertexShader)
      gl.deleteShader(fragmentShader)
      available.value = false
      releaseContext()
      return false
    }
    gl.attachShader(program, vertexShader)
    gl.attachShader(program, fragmentShader)
    gl.linkProgram(program)
    gl.deleteShader(vertexShader)
    gl.deleteShader(fragmentShader)
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      available.value = false
      releaseContext()
      return false
    }

    positionBuffer = gl.createBuffer()
    texcoordBuffer = gl.createBuffer()
    // 记录 attribute/uniform 位置，绘制时只更新每个 tile 的坐标和纹理。
    positionLocation = gl.getAttribLocation(program, 'a_position')
    texcoordLocation = gl.getAttribLocation(program, 'a_texcoord')
    textureLocation = gl.getUniformLocation(program, 'u_texture')
    if (
      !positionBuffer ||
      !texcoordBuffer ||
      positionLocation < 0 ||
      texcoordLocation < 0 ||
      !textureLocation
    ) {
      // Shader 或基础缓冲区初始化失败，统一关闭 tiled 路径。
      available.value = false
      releaseContext()
      return false
    }

    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, 1)
    gl.useProgram(program)
    gl.uniform1i(textureLocation, 0)
    gl.enableVertexAttribArray(positionLocation)
    gl.enableVertexAttribArray(texcoordLocation)
    return gl.getError() === gl.NO_ERROR
  }

  // 让 canvas 的绘制缓冲区匹配 viewport 的 CSS 尺寸和设备像素比。
  function resizeCanvas() {
    const canvas = canvasRef.value
    const viewport = viewportRef.value
    if (!canvas || !viewport || !gl) return

    const dpr = Math.max(window.devicePixelRatio || 1, 1)
    const width = Math.max(1, Math.round(viewport.clientWidth * dpr))
    const height = Math.max(1, Math.round(viewport.clientHeight * dpr))
    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width
      canvas.height = height
    }
    canvas.style.width = `${viewport.clientWidth}px`
    canvas.style.height = `${viewport.clientHeight}px`
    gl.viewport(0, 0, width, height)
  }

  // 清空 canvas，避免上一张图片的残留画面继续可见。
  function clearCanvas() {
    if (!gl || !canvasRef.value) return
    resizeCanvas()
    gl.clearColor(0, 0, 0, 0)
    gl.clear(gl.COLOR_BUFFER_BIT)
  }

  // 将任意角度归一化到 WebGL 绘制使用的 0/90/180/270 范围。
  function normalizeRotation(degrees: number): number {
    return ((degrees % 360) + 360) % 360
  }

  // 把原图坐标映射到旋转后的 stage 坐标。
  function mapSourcePoint(
    x: number,
    y: number,
    width: number,
    height: number,
    scale: number,
    rotation: number
  ): [number, number] {
    switch (rotation) {
      case 90:
        return [(height - y) * scale, x * scale]
      case 180:
        return [(width - x) * scale, (height - y) * scale]
      case 270:
        return [y * scale, (width - x) * scale]
      default:
        return [x * scale, y * scale]
    }
  }

  // 将 viewport 像素坐标转换为 WebGL clip 坐标。
  function toClipPoint(x: number, y: number, width: number, height: number): [number, number] {
    return [(x / width) * 2 - 1, 1 - (y / height) * 2]
  }

  // 根据当前 stage 位置绘制一个 tile，不改变 DOM 布局。
  function drawTile(tile: TileTexture, entry: TileCacheEntry) {
    if (!gl || !program || !positionBuffer || !texcoordBuffer) return
    const viewport = viewportRef.value
    const stage = stageRef.value
    if (!viewport || !stage) return

    const viewportRect = viewport.getBoundingClientRect()
    const stageRect = stage.getBoundingClientRect()
    const canvasWidth = Math.max(viewport.clientWidth, 1)
    const canvasHeight = Math.max(viewport.clientHeight, 1)
    const stageOffsetX = stageRect.left - viewportRect.left
    const stageOffsetY = stageRect.top - viewportRect.top
    const scale = displayScale.value
    const rotation = normalizeRotation(rotationDegrees.value)
    const sourcePoints: Array<[number, number]> = [
      [tile.x, tile.y],
      [tile.x + tile.width, tile.y],
      [tile.x, tile.y + tile.height],
      [tile.x + tile.width, tile.y + tile.height],
    ]
    const positions: number[] = []
    for (const [sourceX, sourceY] of sourcePoints) {
      const [stageX, stageY] = mapSourcePoint(
        sourceX,
        sourceY,
        entry.width,
        entry.height,
        scale,
        rotation
      )
      const [clipX, clipY] = toClipPoint(
        stageOffsetX + stageX,
        stageOffsetY + stageY,
        canvasWidth,
        canvasHeight
      )
      positions.push(clipX, clipY)
    }

    gl.useProgram(program)
    gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer)
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STREAM_DRAW)
    gl.vertexAttribPointer(positionLocation, 2, gl.FLOAT, false, 0, 0)
    gl.bindBuffer(gl.ARRAY_BUFFER, texcoordBuffer)
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([0, 0, 1, 0, 0, 1, 1, 1]), gl.STREAM_DRAW)
    gl.vertexAttribPointer(texcoordLocation, 2, gl.FLOAT, false, 0, 0)
    gl.activeTexture(gl.TEXTURE0)
    gl.bindTexture(gl.TEXTURE_2D, tile.texture)
    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4)
  }

  // 绘制当前完整 tile 集合，并把 context 错误转换成普通图片兜底。
  function renderEntry(entry: TileCacheEntry): boolean {
    if (
      !initializeContext() ||
      !gl ||
      !canvasRef.value ||
      !viewportRef.value ||
      !stageRef.value ||
      entry.tiles.length === 0 ||
      viewportRef.value.clientWidth <= 0 ||
      viewportRef.value.clientHeight <= 0 ||
      !Number.isFinite(displayScale.value) ||
      displayScale.value <= 0
    ) {
      return false
    }

    // 每帧先清空 viewport，再按原图坐标绘制全部 tile。
    resizeCanvas()
    gl.clearColor(0, 0, 0, 0)
    gl.clear(gl.COLOR_BUFFER_BIT)
    for (const tile of entry.tiles) drawTile(tile, entry)

    if (gl.isContextLost() || gl.getError() !== gl.NO_ERROR) {
      available.value = false
      releaseContext()
      onRuntimeFailure?.()
      return false
    }
    return true
  }

  // 合并滚动、缩放和尺寸变化，下一帧统一重绘当前 tile 集合。
  function requestRender() {
    if (renderFrame !== null) return
    renderFrame = window.requestAnimationFrame(() => {
      renderFrame = null
      const entry = visibleEntry
      const painted = entry ? renderEntry(entry) : false
      const waiters = renderWaiters
      renderWaiters = []
      for (const waiter of waiters) {
        waiter.resolve(waiter.entry === entry && painted)
      }
    })
  }

  // 等待当前 canvas 按最新布局完成一帧真实绘制，避免暴露旧 drawing buffer。
  function paintCurrentFrame(): Promise<boolean> {
    const entry = visibleEntry
    if (!entry) return Promise.resolve(false)

    return new Promise((resolve) => {
      renderWaiters.push({ entry, resolve })
      requestRender()
    })
  }

  // 在一帧内上传一个 tile，并在完成后允许 Worker 继续生成下一块。
  function uploadTile(tile: LightboxTile, job: ActiveJob): Promise<void> {
    return new Promise((resolve, reject) => {
      const frame = window.requestAnimationFrame(() => {
        job.cancelPendingUpload = null
        let texture: WebGLTexture | null = null
        try {
          if (activeJob !== job || !gl) {
            tile.bitmap.close()
            resolve()
            return
          }

          texture = gl.createTexture()
          if (!texture) throw new Error('Failed to create lightbox tile texture')
          // 使用 mipmap 进行缩小采样，减少大图缩放时的锯齿和闪烁。
          gl.bindTexture(gl.TEXTURE_2D, texture)
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR)
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR)
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE)
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE)
          gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, tile.bitmap)
          // 每个 tile 独立生成采样层级，不引入整图多分辨率金字塔。
          gl.generateMipmap(gl.TEXTURE_2D)
          if (activeJob !== job) {
            gl.deleteTexture(texture)
            resolve()
            return
          }
          if (gl.isContextLost() || gl.getError() !== gl.NO_ERROR) {
            throw new Error('Failed to upload lightbox tile')
          }

          job.entry.tiles.push({
            x: tile.x,
            y: tile.y,
            width: tile.width,
            height: tile.height,
            texture,
          })
          if (visibleEntry) requestRender()
          resolve()
        } catch (error) {
          if (texture) gl?.deleteTexture(texture)
          reject(error)
        } finally {
          tile.bitmap.close()
        }
      })

      job.cancelPendingUpload = () => {
        window.cancelAnimationFrame(frame)
        job.cancelPendingUpload = null
        tile.bitmap.close()
        resolve()
      }
    })
  }

  // 中止当前显示或预加载任务，并释放尚未完成的 tile。
  function cancelActiveJob() {
    const job = activeJob
    if (!job) return

    activeJob = null
    job.cancelPendingUpload?.()
    job.cancelPendingUpload = null
    tileWorker.cancel()
    releaseEntry(job.entry)
  }

  // 将一条 Worker tile 流上传完成，并校验真实尺寸与 Asset 元数据一致。
  async function uploadRequest(
    request: LightboxTileRequest,
    job: ActiveJob
  ): Promise<TileCacheEntry | undefined> {
    try {
      const streamed = await tileWorker.stream({
        url: request.url,
        tileSize: LIGHTBOX_TILE_SIZE,
        onMeta: (size) => {
          // 元数据不一致时不冒险绘制，直接让上层回退普通 img。
          if (size.width !== request.width || size.height !== request.height) {
            throw new Error(
              `Lightbox tile dimensions do not match metadata: ${size.width}x${size.height}`
            )
          }
        },
        onTile: (tile) => uploadTile(tile, job),
      })

      if (streamed === null || generation !== job.generation || activeJob !== job) {
        return undefined
      }

      cache.set(request.url, job.entry)
      return job.entry
    } catch (error: unknown) {
      if (!job.prefetch) console.warn('Failed to load lightbox tiles:', error)
      return undefined
    }
  }

  // 等待布局落地后绘制一帧，确认 canvas 可以接管画面。
  function waitForPaint(entry: TileCacheEntry, expectedGeneration: number): Promise<boolean> {
    return new Promise((resolve) => {
      window.requestAnimationFrame(() => {
        if (generation !== expectedGeneration || visibleEntry !== entry) {
          resolve(false)
          return
        }
        resolve(renderEntry(entry))
      })
    })
  }

  // 显示当前原图；缓存命中直接绘制，否则启动 Worker 分块上传。
  async function show(request: LightboxTileRequest): Promise<LightboxTileShowResult> {
    const requestGeneration = ++generation
    // 新目标优先，停止旧的预加载或显示上传。
    cancelActiveJob()
    visibleEntry = null
    retainedUrls.add(request.url)

    if (!available.value || !initializeContext()) {
      // WebGL2 不可用时不创建 Worker，直接让上层挂载普通 img。
      return { kind: 'fallback', reason: 'webgl-unavailable' }
    }

    let entry = cache.get(request.url)
    if (entry && (entry.width !== request.width || entry.height !== request.height)) {
      // 同 URL 的尺寸变化表示资源版本已变，不能复用旧 texture。
      releaseEntry(entry)
      cache.delete(request.url)
      entry = undefined
    }

    if (!entry) {
      const job: ActiveJob = {
        generation: requestGeneration,
        request,
        entry: { url: request.url, width: request.width, height: request.height, tiles: [] },
        prefetch: false,
        cancelPendingUpload: null,
      }
      activeJob = job
      // 当前图必须等全部 tile 上传并完成一帧绘制后才切换可见层。
      entry = await uploadRequest(request, job)
      if (activeJob === job) activeJob = null
      if (!entry) releaseEntry(job.entry)
    }

    if (generation !== requestGeneration) return { kind: 'stale' }
    if (!entry) {
      // 当前 tile 流失败，交给普通 img 重新加载原图。
      return { kind: 'fallback', reason: 'tile-upload-failed' }
    }

    visibleEntry = entry
    if (!(await waitForPaint(entry, requestGeneration))) {
      visibleEntry = null
      return generation === requestGeneration
        ? { kind: 'fallback', reason: 'tile-render-failed' }
        : { kind: 'stale' }
    }

    pumpPrefetch()
    return { kind: 'ready' }
  }

  // 串行上传保留范围内的相邻大图，避免和当前图争用 Worker。
  function pumpPrefetch() {
    if (activeJob || prefetchQueue.length === 0 || !available.value || !initializeContext()) return

    const request = prefetchQueue.shift()!
    if (!retainedUrls.has(request.url) || cache.has(request.url)) {
      // 选择范围已变化或缓存已命中时跳过这项预热。
      pumpPrefetch()
      return
    }

    const job: ActiveJob = {
      generation,
      request,
      entry: { url: request.url, width: request.width, height: request.height, tiles: [] },
      prefetch: true,
      cancelPendingUpload: null,
    }
    activeJob = job
    void uploadRequest(request, job).then((entry) => {
      if (activeJob !== job) return
      activeJob = null
      // 预热失败只丢弃半成品，不影响当前已经显示的图片。
      if (!entry) releaseEntry(job.entry)
      pruneCache()
      pumpPrefetch()
    })
  }

  // 将一个相邻大图加入预加载队列，重复请求直接忽略。
  function prefetch(request: LightboxTileRequest) {
    if (
      !request.url ||
      !available.value ||
      cache.has(request.url) ||
      activeJob?.request.url === request.url ||
      prefetchQueue.some((item) => item.url === request.url)
    ) {
      return
    }
    prefetchQueue.push(request)
    pumpPrefetch()
  }

  // 按当前选择范围删除不再需要的完整缓存和预加载队列项。
  function pruneCache() {
    for (const [url, entry] of cache) {
      if (retainedUrls.has(url) || visibleEntry === entry || activeJob?.entry === entry) continue
      releaseEntry(entry)
      cache.delete(url)
    }
    prefetchQueue = prefetchQueue.filter((request) => retainedUrls.has(request.url))
  }

  // 更新当前图及相邻图的保留范围，范围外的 texture 立即释放。
  function retain(urls: string[]) {
    // 先更新保留集合，再取消已不在范围内的相邻预热。
    retainedUrls = new Set(urls)
    if (activeJob?.prefetch && !retainedUrls.has(activeJob.request.url)) {
      cancelActiveJob()
    }
    pruneCache()
  }

  // 关闭 tiled renderer，取消任务并释放所有 GPU、Worker 和缓存资源。
  function close() {
    // 关闭时让所有异步 continuation 失效，再释放 Worker 和 GPU 资源。
    generation += 1
    cancelActiveJob()
    prefetchQueue = []
    visibleEntry = null
    const waiters = renderWaiters
    renderWaiters = []
    for (const waiter of waiters) waiter.resolve(false)
    retainedUrls.clear()
    releaseCache()
    clearCanvas()
  }

  // WebGL context 丢失时清理资源，并通知上层改用普通 img。
  function handleContextLost(event: Event) {
    event.preventDefault()
    available.value = false
    close()
    releaseContext()
    onRuntimeFailure?.()
  }

  // context 恢复后允许下一次 show 重新初始化 WebGL。
  function handleContextRestored() {
    // GPU 资源已在 context lost 时释放；下一次 show 会重新初始化并上传。
    available.value = true
  }

  // viewport 滚动改变了 tile 相对位置，安排下一帧重绘。
  function handleViewportScroll() {
    requestRender()
  }

  onMounted(() => {
    canvasRef.value?.addEventListener('webglcontextlost', handleContextLost)
    canvasRef.value?.addEventListener('webglcontextrestored', handleContextRestored)
    viewportRef.value?.addEventListener('scroll', handleViewportScroll, { passive: true })
    if (viewportRef.value) {
      resizeObserver = new ResizeObserver(requestRender)
      resizeObserver.observe(viewportRef.value)
    }
  })

  watch([displayScale, rotationDegrees], requestRender)
  watch(viewportRef, (next, previous) => {
    previous?.removeEventListener('scroll', handleViewportScroll)
    next?.addEventListener('scroll', handleViewportScroll, { passive: true })
    requestRender()
  })

  onUnmounted(() => {
    canvasRef.value?.removeEventListener('webglcontextlost', handleContextLost)
    canvasRef.value?.removeEventListener('webglcontextrestored', handleContextRestored)
    viewportRef.value?.removeEventListener('scroll', handleViewportScroll)
    resizeObserver?.disconnect()
    resizeObserver = null
    if (renderFrame !== null) window.cancelAnimationFrame(renderFrame)
    renderFrame = null
    close()
    releaseContext()
    tileWorker.dispose()
  })

  return {
    show,
    paintCurrentFrame,
    prefetch,
    retain,
    close,
  }
}
