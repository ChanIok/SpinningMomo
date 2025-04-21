<script setup lang="ts">
import { ref, watch, onMounted, onUnmounted } from 'vue'
import { useScreenshotListStore } from '@/stores/screenshot-list'
import { useUIStore } from '@/stores'
import type { Screenshot } from '@/types/screenshot'
import ScreenshotGrid from '@/views/screenshot/components/ScreenshotGrid.vue'
import ScreenshotList from '@/views/screenshot/components/ScreenshotList.vue'
import ScreenshotPreview from '@/components/screenshot/ScreenshotPreview.vue'

const props = defineProps<{
  folderId?: string
  albumId?: number
  year?: number
  month?: number
}>()

// 使用简单的alert替代NaiveUI的message
const showError = (msg: string) => alert(msg)
const store = useScreenshotListStore()
const uiStore = useUIStore()
const browserContentRef = ref<HTMLElement | null>(null)

// 预览相关的状态
const showPreview = ref(false)
const previewIndex = ref(0)

// 加载截图
async function loadScreenshots() {
  try {
    await store.loadScreenshots({
      folderId: props.folderId,
      albumId: props.albumId,
      year: props.year,
      month: props.month,
    })
  } catch (error) {
    showError('加载截图失败')
  }
}

// 检测滚动位置并触发加载更多
const handleScroll = () => {
  const container = browserContentRef.value
  if (!container) return

  const scrollBottom = container.scrollTop + container.clientHeight
  const threshold = container.scrollHeight - 800 // 距离底部800px时加载更多

  if (scrollBottom > threshold) {
    loadScreenshots()
  }
}

// 监听属性变化
watch(
  () => [props.folderId, props.albumId, props.year, props.month],
  () => {
    store.reset()
    loadScreenshots()
  },
  { immediate: true }
)

// 设置滚动监听器
onMounted(() => {
  if (browserContentRef.value) {
    browserContentRef.value.addEventListener('scroll', handleScroll)
  }
})

// 清理滚动监听器
onUnmounted(() => {
  if (browserContentRef.value) {
    browserContentRef.value.removeEventListener('scroll', handleScroll)
  }
})

// 处理截图点击事件
function handleScreenshotClick(screenshot: Screenshot) {
  // 找到点击的截图在数组中的索引
  const index = store.screenshots.findIndex(s => s.id === screenshot.id)
  if (index !== -1) {
    previewIndex.value = index
    showPreview.value = true
  }
}
</script>

<template>
  <div
    id="screenshot-browser"
    class="h-full flex flex-col mr-2 bg-white dark:bg-gray-800 rounded-md"
  >
    <!-- 预览组件 -->
    <screenshot-preview
      v-model="showPreview"
      v-model:currentIndex="previewIndex"
      :screenshots="store.screenshots"
      :initial-index="previewIndex"
    />

    <div ref="browserContentRef" class="flex-1 overflow-y-auto">
      <screenshot-grid
        v-if="uiStore.viewMode === 'grid'"
        :screenshots="store.screenshots"
        :loading="store.loading"
        :has-more="store.hasMore"
        :selected-id="showPreview ? store.screenshots[previewIndex]?.id : undefined"
        @load-more="loadScreenshots"
        @screenshot-click="handleScreenshotClick"
      />

      <screenshot-list
        v-else
        :screenshots="store.screenshots"
        :loading="store.loading"
        :has-more="store.hasMore"
        @load-more="loadScreenshots"
      />
    </div>
  </div>
</template>
