<script setup lang="ts">
import { ref, watch, onMounted, onUnmounted } from 'vue'
import { NSpace, NButton, NIcon } from 'naive-ui'
import { GridOutline, ListOutline } from '@vicons/ionicons5'
import ScreenshotGallery from '@/components/screenshot/ScreenshotGallery.vue'
import { useScreenshotStore } from '@/stores/screenshot'

const viewMode = ref<'grid' | 'list'>('grid')
const screenshotStore = useScreenshotStore()
const browserContentRef = ref<HTMLElement | null>(null)

// 检测滚动位置并触发加载更多
const handleScroll = () => {
  const container = browserContentRef.value;
  if (!container) return;

  // 如果正在加载或没有更多内容，则不触发
  if (screenshotStore.loading || screenshotStore.reachedEnd) {
    console.log('跳过加载 - loading:', screenshotStore.loading, 'reachedEnd:', screenshotStore.reachedEnd);
    return;
  }

  const scrollBottom = container.scrollTop + container.clientHeight;
  const threshold = container.scrollHeight - 800; // 距离底部800px时加载更多

  console.log('滚动指标:', {
    scrollTop: container.scrollTop,
    clientHeight: container.clientHeight,
    scrollHeight: container.scrollHeight,
    scrollBottom,
    threshold,
    diff: threshold - scrollBottom
  });

  if (scrollBottom > threshold) {
    console.log('触发加载更多');
    handleLoadMore();
  }
};

// 设置滚动监听器
onMounted(() => {
  if (browserContentRef.value) {
    browserContentRef.value.addEventListener('scroll', handleScroll);
  }
});

// 清理滚动监听器
onUnmounted(() => {
  if (browserContentRef.value) {
    browserContentRef.value.removeEventListener('scroll', handleScroll);
  }
});

// 处理加载更多的事件
const handleLoadMore = () => {
  console.log('loadMore triggered, store state:', {
    loading: screenshotStore.loading,
    reachedEnd: screenshotStore.reachedEnd,
    screenshotsCount: screenshotStore.screenshots.length
  });
  screenshotStore.loadMoreScreenshots()
}

// 监听 reachedEnd 的变化
watch(() => screenshotStore.reachedEnd, (newVal) => {
  console.log('reachedEnd changed:', newVal);
});

// 初始加载
console.log('initial load, store state:', {
  loading: screenshotStore.loading,
  reachedEnd: screenshotStore.reachedEnd,
  screenshotsCount: screenshotStore.screenshots.length
});
screenshotStore.loadMoreScreenshots()
</script>

<template>
  <div class="screenshot-browser">
    <div class="browser-toolbar">
      <n-space align="center" :size="12">
        <div class="view-mode-buttons">
          <n-button
            quaternary
            size="small"
            :type="viewMode === 'grid' ? 'primary' : 'default'"
            @click="viewMode = 'grid'"
          >
            <template #icon>
              <n-icon><grid-outline /></n-icon>
            </template>
          </n-button>
          <n-button
            quaternary
            size="small"
            :type="viewMode === 'list' ? 'primary' : 'default'"
            @click="viewMode = 'list'"
          >
            <template #icon>
              <n-icon><list-outline /></n-icon>
            </template>
          </n-button>
        </div>
      </n-space>
    </div>
    <div 
      ref="browserContentRef"
      class="browser-content"
    >
      <screenshot-gallery
        v-if="viewMode === 'grid'"
        :screenshots="screenshotStore.screenshots"
        :loading="screenshotStore.loading"
        :has-more="!screenshotStore.reachedEnd"
        @load-more="handleLoadMore"
      />
      <!-- 列表视图将在后续实现 -->
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

.browser-toolbar {
  flex: none;
  padding: 12px 16px;
  border-bottom: 1px solid var(--n-border-color);
  background-color: var(--n-color);
  position: sticky;
  top: 0;
  z-index: 1;
}

.view-mode-buttons {
  display: flex;
  gap: 4px;
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
