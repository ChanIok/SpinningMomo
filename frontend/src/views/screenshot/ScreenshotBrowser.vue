<script setup lang="ts">
import { ref, watch, onMounted, onUnmounted } from 'vue'
import { NSpace, NButton, NIcon } from 'naive-ui'
import { GridOutline, ListOutline } from '@vicons/ionicons5'
import ScreenshotGallery from '@/components/screenshot/ScreenshotGallery.vue'
import { useScreenshotListStore } from '@/stores/screenshot-list'

const props = defineProps<{
  year?: string;
  month?: string;
}>();

const viewMode = ref<'grid' | 'list'>('grid')
const listStore = useScreenshotListStore()
const browserContentRef = ref<HTMLElement | null>(null)

// 检测滚动位置并触发加载更多
const handleScroll = () => {
  const container = browserContentRef.value;
  if (!container) return;

  const scrollBottom = container.scrollTop + container.clientHeight;
  const threshold = container.scrollHeight - 800; // 距离底部800px时加载更多

  if (scrollBottom > threshold) {
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
  if (props.year && props.month) {
    console.log('Loading screenshots for month:', props.year, props.month);
    listStore.loadByMonth(parseInt(props.year), parseInt(props.month));
  } else {
    console.log('Loading more screenshots');
    listStore.loadMore();
  }
};

// 监听路由参数变化
watch(
  () => [props.year, props.month],
  () => {
    listStore.reset();
    if (props.year && props.month) {
      listStore.loadByMonth(parseInt(props.year), parseInt(props.month), true);
    } else {
      listStore.loadMore();
    }
  },
  { immediate: true }
);
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
        :screenshots="listStore.screenshots"
        :loading="listStore.loading"
        :has-more="listStore.hasMore"
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
