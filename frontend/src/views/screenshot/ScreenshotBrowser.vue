<script setup lang="ts">
import { ref, watch, onMounted, onUnmounted } from 'vue'
import { useScreenshotListStore } from '@/stores/screenshot-list'
import type { ScreenshotParams } from '@/types/screenshot'
import ViewToolbar from '@/views/screenshot/components/ViewToolbar.vue'
import ScreenshotGrid from '@/views/screenshot/components/ScreenshotGrid.vue'
import ScreenshotList from '@/views/screenshot/components/ScreenshotList.vue'
import { useMessage } from 'naive-ui'

const props = defineProps<{
  folderId?: string
  albumId?: number
  year?: number
  month?: number
}>()

const message = useMessage()
const store = useScreenshotListStore()
const viewMode = ref<'grid' | 'list'>('grid')
const browserContentRef = ref<HTMLElement | null>(null)

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
    message.error('加载截图失败')
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
</script>

<template>
  <div class="screenshot-browser">
    <view-toolbar v-model:mode="viewMode" />
    
    <div 
      ref="browserContentRef"
      class="browser-content"
    >
      <screenshot-grid 
        v-if="viewMode === 'grid'"
        :screenshots="store.screenshots"
        :loading="store.loading"
        :has-more="store.hasMore"
        @load-more="loadScreenshots"
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

<style scoped>
.screenshot-browser {
  height: 100%;
  display: flex;
  flex-direction: column;
  background-color: var(--n-color);
}

.browser-content {
  flex: 1;
  padding: 16px;
  overflow-y: auto;
  background-color: var(--n-color);
}

@media (max-width: 600px) {
  .browser-content {
    padding: 8px;
  }
}
</style>
