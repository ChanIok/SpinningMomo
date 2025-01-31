<script setup lang="ts">
import { ref } from 'vue'
import { NSpace, NButton, NIcon } from 'naive-ui'
import { GridOutline, ListOutline } from '@vicons/ionicons5'
import ScreenshotGrid from '@/components/screenshot/ScreenshotGrid.vue'
import { useScreenshotStore } from '@/stores/screenshot'

const viewMode = ref<'grid' | 'list'>('grid')
const screenshotStore = useScreenshotStore()

// 初始加载
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
    <div class="browser-content">
      <screenshot-grid
        v-if="viewMode === 'grid'"
        :screenshots="screenshotStore.screenshots"
        :loading="screenshotStore.loading"
        :has-more="!screenshotStore.reachedEnd"
      />
      <!-- 列表视图组件将在后续实现 -->
    </div>
  </div>
</template>

<style scoped>
.screenshot-browser {
  height: 100%;
  display: flex;
  flex-direction: column;
}

.browser-toolbar {
  flex: none;
  padding: 12px 16px;
  border-bottom: 1px solid var(--n-border-color);
  background-color: var(--n-color);
}

.view-mode-buttons {
  display: flex;
  gap: 4px;
}

.browser-content {
  flex: 1;
  padding: 16px;
}
</style>
