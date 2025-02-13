import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { AppSettings } from '@/types/settings'
import { settingsAPI } from '@/api/settings'

export const useSettingsStore = defineStore('settings', () => {
    // State
    const settings = ref<AppSettings | null>(null)
    const loading = ref(false)

    // Actions
    async function loadSettings() {
        try {
            loading.value = true
            settings.value = await settingsAPI.getSettings()
        } catch (error) {
            throw error
        } finally {
            loading.value = false
        }
    }

    async function saveSettings() {
        if (!settings.value) return

        try {
            loading.value = true
            settings.value = await settingsAPI.updateSettings(settings.value)
        } catch (error) {
            throw error
        } finally {
            loading.value = false
        }
    }

    async function addWatchedFolder(path: string) {
        if (!settings.value) return

        try {
            loading.value = true
            const folder = await settingsAPI.addWatchedFolder({
                path,
                include_subfolders: true,
                last_scan: new Date().toISOString()
            })
            settings.value.watched_folders.push(folder)
        } catch (error) {
            throw error
        } finally {
            loading.value = false
        }
    }

    async function removeWatchedFolder(path: string) {
        if (!settings.value) return

        try {
            loading.value = true
            await settingsAPI.removeWatchedFolder(path)
            settings.value.watched_folders = settings.value.watched_folders.filter(
                f => f.path !== path
            )
        } catch (error) {
            throw error
        } finally {
            loading.value = false
        }
    }

    function updateWatchedFolders(folders: AppSettings['watched_folders']) {
        if (settings.value) {
            settings.value.watched_folders = folders
        }
    }

    function updateThumbnailSettings(thumbnailSettings: AppSettings['thumbnails']) {
        if (settings.value) {
            settings.value.thumbnails = thumbnailSettings
        }
    }

    function updateInterfaceSettings(interfaceSettings: AppSettings['interface']) {
        if (settings.value) {
            settings.value.interface = interfaceSettings
        }
    }

    function updatePerformanceSettings(performanceSettings: AppSettings['performance']) {
        if (settings.value) {
            settings.value.performance = performanceSettings
        }
    }

    function reset() {
        settings.value = null
        loading.value = false
    }

    return {
        // State
        settings,
        loading,

        // Actions
        loadSettings,
        saveSettings,
        addWatchedFolder,
        removeWatchedFolder,
        updateWatchedFolders,
        updateThumbnailSettings,
        updateInterfaceSettings,
        updatePerformanceSettings,
        reset
    }
}) 