<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useSettingsStore } from '@/stores/settings'
import type { AppSettings } from '@/types/settings'
import { 
  NCard, 
  NTabs, 
  NTabPane, 
  NSpin,
  NSpace,
  NButton,
  useMessage
} from 'naive-ui'
import WatchedFolders from './components/WatchedFolders.vue'
import ThumbnailSettings from './components/ThumbnailSettings.vue'
import InterfaceSettings from './components/InterfaceSettings.vue'
import PerformanceSettings from './components/PerformanceSettings.vue'

const settingsStore = useSettingsStore()
const message = useMessage()
const activeTab = ref('folders')

// 加载设置
onMounted(async () => {
  try {
    await settingsStore.loadSettings()
  } catch (error) {
    message.error('加载设置失败')
  }
})

// 保存设置
async function handleSave() {
  try {
    await settingsStore.saveSettings()
    message.success('设置已保存')
  } catch (error) {
    message.error('保存设置失败')
  }
}

// 处理监视文件夹更新
function handleWatchedFoldersUpdate(folders: AppSettings['watched_folders']) {
  settingsStore.updateWatchedFolders(folders)
}

// 处理缩略图设置更新
function handleThumbnailSettingsUpdate(settings: AppSettings['thumbnails']) {
  settingsStore.updateThumbnailSettings(settings)
}

// 处理界面设置更新
function handleInterfaceSettingsUpdate(settings: AppSettings['interface']) {
  settingsStore.updateInterfaceSettings(settings)
}

// 处理性能设置更新
function handlePerformanceSettingsUpdate(settings: AppSettings['performance']) {
  settingsStore.updatePerformanceSettings(settings)
}
</script>

<template>
  <div class="settings-view">
    <n-card title="设置">
      <template #header-extra>
        <n-button
          type="primary"
          :loading="settingsStore.loading"
          @click="handleSave"
        >
          保存设置
        </n-button>
      </template>

      <n-spin :show="settingsStore.loading">
        <n-tabs v-model:value="activeTab" type="line" animated>
          <n-tab-pane name="folders" tab="监视文件夹">
            <watched-folders 
              v-if="settingsStore.settings"
              :folders="settingsStore.settings.watched_folders"
              @update="handleWatchedFoldersUpdate"
            />
          </n-tab-pane>

          <n-tab-pane name="thumbnail" tab="缩略图">
            <thumbnail-settings
              v-if="settingsStore.settings"
              :settings="settingsStore.settings.thumbnails"
              @update="handleThumbnailSettingsUpdate"
            />
          </n-tab-pane>

          <n-tab-pane name="interface" tab="界面">
            <interface-settings
              v-if="settingsStore.settings"
              :settings="settingsStore.settings.interface"
              @update="handleInterfaceSettingsUpdate"
            />
          </n-tab-pane>

          <n-tab-pane name="performance" tab="性能">
            <performance-settings
              v-if="settingsStore.settings"
              :settings="settingsStore.settings.performance"
              @update="handlePerformanceSettingsUpdate"
            />
          </n-tab-pane>
        </n-tabs>
      </n-spin>
    </n-card>
  </div>
</template>

<style scoped>
.settings-view {
  padding: 20px;
  height: 100%;
  overflow: auto;
}
</style>