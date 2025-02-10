import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { AppSettings, WatchedFolder } from '@/types/settings'
import { settingsAPI } from '@/api/settings'

// 定义操作结果类型
interface OperationResult<T = void> {
    success: boolean
    data?: T
    message?: string  // 成功时的消息
    error?: string    // 失败时的消息
}

export const useSettingsStore = defineStore('settings', () => {
    const settings = ref<AppSettings | null>(null)
    const loading = ref(false)

    // 加载设置
    async function loadSettings(): Promise<OperationResult<AppSettings>> {
        try {
            loading.value = true
            const data = await settingsAPI.getSettings()
            settings.value = data
            return { success: true, data }
        } catch (error) {
            return { 
                success: false, 
                error: error instanceof Error ? error.message : '加载设置失败'
            }
        } finally {
            loading.value = false
        }
    }

    // 添加监视文件夹
    async function addWatchedFolder(folder: WatchedFolder): Promise<OperationResult<WatchedFolder>> {
        try {
            loading.value = true
            const response = await settingsAPI.addWatchedFolder(folder)
            if (response.success && settings.value) {
                settings.value.watched_folders.push(response.data)
                return { success: true, data: response.data }
            }
            return { 
                success: false, 
                error: '添加文件夹失败'
            }
        } catch (error) {
            return { 
                success: false, 
                error: error instanceof Error ? error.message : '添加文件夹失败'
            }
        } finally {
            loading.value = false
        }
    }

    // 删除监视文件夹
    async function removeWatchedFolder(path: string): Promise<OperationResult> {
        try {
            loading.value = true
            const response = await settingsAPI.removeWatchedFolder(path)
            
            if (response.success) {
                if (settings.value) {
                    settings.value.watched_folders = settings.value.watched_folders.filter(
                        folder => folder.path !== path
                    )
                }
                return { 
                    success: true, 
                    message: response.data.message
                }
            } else {
                return { 
                    success: false, 
                    error: response.data.message
                }
            }
        } catch (error) {
            return { 
                success: false, 
                error: error instanceof Error ? error.message : '删除文件夹失败'
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