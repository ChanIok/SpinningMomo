#pragma once

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useSettingsStore } from '@/stores'
import { settingsAPI } from '@/api/settings'
import { 
    NCard, 
    NTabs, 
    NTabPane, 
    NSpin,
    NSpace,
    NButton,
    useMessage
} from 'naive-ui'
import WatchedFolders from '@/views/settings/components/WatchedFolders.vue'
import ThumbnailSettings from '@/views/settings/components/ThumbnailSettings.vue'
import InterfaceSettings from '@/views/settings/components/InterfaceSettings.vue'
import PerformanceSettings from '@/views/settings/components/PerformanceSettings.vue'

const settingsStore = useSettingsStore()
const message = useMessage()
const activeTab = ref('folders')

// 加载设置
onMounted(async () => {
    const result = await settingsStore.loadSettings()
    if (!result.success) {
        message.error(result.error || '加载设置失败')
    }
})

// 手动保存设置
async function handleSave() {
    try {
        if (!settingsStore.settings) {
            message.error('没有可保存的设置')
            return
        }
        const response = await settingsAPI.updateSettings(settingsStore.settings)
        message.success('设置已保存')
    } catch (error) {
        message.error(error instanceof Error ? error.message : '保存设置失败')
    }
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
                        />
                    </n-tab-pane>

                    <n-tab-pane name="thumbnail" tab="缩略图">
                        <thumbnail-settings
                            v-if="settingsStore.settings"
                            v-model:settings="settingsStore.settings.thumbnails"
                        />
                    </n-tab-pane>

                    <n-tab-pane name="interface" tab="界面">
                        <interface-settings
                            v-if="settingsStore.settings"
                            v-model:settings="settingsStore.settings.interface"
                        />
                    </n-tab-pane>

                    <n-tab-pane name="performance" tab="性能">
                        <performance-settings
                            v-if="settingsStore.settings"
                            v-model:settings="settingsStore.settings.performance"
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