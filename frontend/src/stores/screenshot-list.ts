import { defineStore } from 'pinia';
import { ref, computed } from 'vue';
import type { Screenshot, ScreenshotParams } from '@/types/screenshot';
import { screenshotAPI } from '@/api/screenshot';

export const useScreenshotListStore = defineStore('screenshotList', () => {
    // State
    const screenshots = ref<Screenshot[]>([]);
    const loading = ref(false);
    const hasMore = ref(true);
    const lastId = ref<number | null>(null);
    const error = ref<string | null>(null);

    // Getters
    const isEmpty = computed(() => screenshots.value.length === 0);

    // Actions
    async function loadScreenshots(params?: Partial<ScreenshotParams>) {
        if (loading.value || (!hasMore.value && !params?.reset)) return;

        try {
            loading.value = true;
            error.value = null;

            const result = await screenshotAPI.getScreenshots({
                lastId: params?.reset ? undefined : lastId.value?.toString(),
                limit: 20,
                ...params
            });

            if (params?.reset) {
                screenshots.value = result.items;
            } else {
                screenshots.value.push(...result.items);
            }

            hasMore.value = result.hasMore;
            if (result.items.length > 0) {
                lastId.value = result.items[result.items.length - 1].id;
            }
        } catch (err) {
            error.value = err instanceof Error ? err.message : '未知错误';
            throw err;
        } finally {
            loading.value = false;
        }
    }

    function removeScreenshot(id: number) {
        screenshots.value = screenshots.value.filter(s => s.id !== id);
    }

    function reset() {
        screenshots.value = [];
        loading.value = false;
        hasMore.value = true;
        lastId.value = null;
        error.value = null;
    }

    return {
        // State
        screenshots,
        loading,
        hasMore,
        lastId,
        error,
        
        // Getters
        isEmpty,
        
        // Actions
        loadScreenshots,
        removeScreenshot,
        reset
    };
}); 