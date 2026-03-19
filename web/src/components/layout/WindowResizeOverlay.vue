<script setup lang="ts">
import { computed } from 'vue'
import { isWebView } from '@/core/env'

type ResizeEdge =
  | 'top'
  | 'right'
  | 'bottom'
  | 'left'
  | 'topLeft'
  | 'topRight'
  | 'bottomLeft'
  | 'bottomRight'

type ResizeHandle = {
  edge: ResizeEdge
  className: string
  cursor: string
}

const showResizeOverlay = computed(() => isWebView())

const resizeHandles: ResizeHandle[] = [
  { edge: 'top', className: 'left-0 top-0 h-1.5 w-full', cursor: 'n-resize' },
  { edge: 'right', className: 'right-0 top-0 h-full w-1.5', cursor: 'e-resize' },
  { edge: 'bottom', className: 'bottom-0 left-0 h-1.5 w-full', cursor: 's-resize' },
  { edge: 'left', className: 'left-0 top-0 h-full w-1.5', cursor: 'w-resize' },
  { edge: 'topLeft', className: 'left-0 top-0 h-3 w-3', cursor: 'nw-resize' },
  { edge: 'topRight', className: 'right-0 top-0 h-3 w-3', cursor: 'ne-resize' },
  { edge: 'bottomLeft', className: 'bottom-0 left-0 h-3 w-3', cursor: 'sw-resize' },
  { edge: 'bottomRight', className: 'bottom-0 right-0 h-3 w-3', cursor: 'se-resize' },
]

function handleMouseDown(edge: ResizeEdge, event: MouseEvent): void {
  if (event.button !== 0) {
    return
  }

  event.preventDefault()
  event.stopPropagation()

  if (!window.chrome?.webview) {
    return
  }

  window.chrome.webview.postMessage({ type: 'window.beginResize', edge })
}
</script>

<template>
  <div v-if="showResizeOverlay" class="pointer-events-none absolute inset-0 z-500 select-none">
    <button
      v-for="handle in resizeHandles"
      :key="handle.edge"
      type="button"
      tabindex="-1"
      aria-hidden="true"
      class="pointer-events-auto absolute border-0 bg-transparent p-0 opacity-0"
      :class="handle.className"
      :style="{ cursor: handle.cursor }"
      @mousedown="(event) => handleMouseDown(handle.edge, event)"
    />
  </div>
</template>
