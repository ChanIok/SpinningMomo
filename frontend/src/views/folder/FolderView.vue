<script setup lang="ts">
import { ref, watch } from 'vue'
import { NFlex } from 'naive-ui'
import { useScreenshotStore } from '@/stores/screenshot'
import ScreenshotBrowser from '@/views/screenshot/ScreenshotBrowser.vue'
import FolderTree from './components/FolderTree.vue'

const currentFolderId = ref<string>('')
const currentRelativePath = ref<string>('')
const screenshotStore = useScreenshotStore()

// 处理文件夹选择
const handleFolderSelect = (folderId: string, relativePath: string) => {
  currentFolderId.value = folderId
  currentRelativePath.value = relativePath
  screenshotStore.reset()
}
</script>

<template>
  <n-flex :size="24" style="height: 100%">
    <div class="folder-tree-container">
      <folder-tree @select="handleFolderSelect" />
    </div>
    <div class="screenshot-browser-container">
      <screenshot-browser
        :folder-id="currentFolderId"
        :relative-path="currentRelativePath"
      />
    </div>
  </n-flex>
</template>

<style scoped>
.folder-tree-container {
  width: 280px;
  height: 100%;
  border-right: 1px solid var(--n-border-color);
  overflow-y: auto;
}

.screenshot-browser-container {
  flex: 1;
  height: 100%;
  overflow: hidden;
}
</style> 