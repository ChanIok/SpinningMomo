import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { AppSettings, WatchedFolder } from '@/types/settings'
import { settingsAPI } from '@/api/settings'

export const useSettingsStore = defineStore('settings', () => {
    const settings = ref<AppSettings | null>(null)
    const loading = ref(false)

    // 加载设置
    async function loadSettings(): Promise<void> {
        try {
            loading.value = true
            const response = await settingsAPI.getSettings()
            settings.value = response
        } finally {
            loading.value = false
        }
    }

    // 添加监视文件夹
    async function addWatchedFolder(folder: WatchedFolder): Promise<void> {
        try {
            loading.value = true
            const response = await settingsAPI.addWatchedFolder(folder)
            if (settings.value) {
                settings.value.watched_folders.push(folder)
            }
        } finally {
            loading.value = false
        }
    }

    // 删除监视文件夹
    async function removeWatchedFolder(path: string): Promise<void> {
        try {
            loading.value = true
            await settingsAPI.removeWatchedFolder(path)
            
            if (settings.value) {
                settings.value.watched_folders = settings.value.watched_folders.filter(
                    folder => folder.path !== path
                )
            }
        } finally {
            loading.value = false
        }
    }

    return {
        settings,
        loading,
        loadSettings,
        addWatchedFolder,
        removeWatchedFolder
    }
}) 