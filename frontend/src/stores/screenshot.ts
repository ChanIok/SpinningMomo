import { defineStore } from 'pinia';
import { ref, computed } from 'vue';
import type { Screenshot } from '../types/screenshot';
import { screenshotAPI } from '../api/screenshot';

export const useScreenshotStore = defineStore('screenshot', () => {
    // State
    const screenshots = ref<Screenshot[]>([]);
    const currentScreenshot = ref<Screenshot | null>(null);
    const loading = ref(false);
    const reachedEnd = ref(false);
    const lastId = ref<number | null>(null);
    
    // 配置
    const batchSize = 50;

    // Getters
    const hasScreenshots = computed(() => screenshots.value.length > 0);

    // Actions
    async function loadMoreScreenshots() {
        if (loading.value || reachedEnd.value) return;

        loading.value = true;
        try {
            const response = await screenshotAPI.getScreenshots({
                lastId: lastId.value,
                limit: batchSize
            });

            if (response.screenshots.length < batchSize) {
                reachedEnd.value = true;
            }

            if (response.screenshots.length > 0) {
                screenshots.value.push(...response.screenshots);
                lastId.value = response.screenshots[response.screenshots.length - 1].id;
            }
        } catch (error) {
            console.error('Failed to load screenshots:', error);
        } finally {
            loading.value = false;
        }
    }

    async function fetchScreenshot(id: number) {
        loading.value = true;
        try {
            currentScreenshot.value = await screenshotAPI.getScreenshot(id);
            return currentScreenshot.value;
        } catch (error) {
            console.error('Failed to fetch screenshot:', error);
            currentScreenshot.value = null;
            throw error;
        } finally {
            loading.value = false;
        }
    }

    async function deleteScreenshot(id: number) {
        try {
            await screenshotAPI.deleteScreenshot(id);
            screenshots.value = screenshots.value.filter(s => s.id !== id);
            if (currentScreenshot.value?.id === id) {
                currentScreenshot.value = null;
            }
        } catch (error) {
            console.error('Failed to delete screenshot:', error);
            throw error;
        }
    }

    function reset() {
        screenshots.value = [];
        loading.value = false;
        reachedEnd.value = false;
        lastId.value = null;
    }

    return {
        // State
        screenshots,
        currentScreenshot,
        loading,
        reachedEnd,
        
        // Getters
        hasScreenshots,
        
        // Actions
        loadMoreScreenshots,
        fetchScreenshot,
        deleteScreenshot,
        reset
    };
}); 