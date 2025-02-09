#pragma once

<script setup lang="ts">
import { ref, onMounted, watch } from 'vue'
import { useSettingsStore } from '@/stores'
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
const autoSaveTimeout = ref<number | null>(null)
const isInitialLoad = ref(true)

// 加载设置
onMounted(async () => {
    const result = await settingsStore.loadSettings()
    console.log('加载设置结果:', result)
    if (!result.success) {
        message.error(result.error || '加载设置失败')
    }
    isInitialLoad.value = false
})

// 监听设置变化，自动保存
watch(
    () => settingsStore.settings,
    async () => {
        if (isInitialLoad.value) {
            return
        }

        if (autoSaveTimeout.value) {
            window.clearTimeout(autoSaveTimeout.value)
        }
        
        autoSaveTimeout.value = window.setTimeout(async () => {
            const result = await settingsStore.saveSettings()
            if (result.success) {
                message.success('设置已自动保存')
            } else {
                message.error(result.error || '自动保存失败')
            }
        }, 1000)
    },
    { deep: true }
)

// 手动保存设置
async function handleSave() {
    if (autoSaveTimeout.value) {
        window.clearTimeout(autoSaveTimeout.value)
        autoSaveTimeout.value = null
    }
    const result = await settingsStore.saveSettings()
    if (result.success) {
        message.success('设置已保存')
    } else {
        message.error(result.error || '保存设置失败')
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