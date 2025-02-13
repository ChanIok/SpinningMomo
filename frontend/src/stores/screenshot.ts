import { defineStore } from 'pinia';
import { ref, computed } from 'vue';
import type { Screenshot } from '@/types/screenshot';

export const useScreenshotStore = defineStore('screenshot', () => {
    // State
    const screenshots = ref<Screenshot[]>([]);
    const currentScreenshot = ref<Screenshot | null>(null);
    const loading = ref(false);
    const hasMore = ref(true);
    const lastId = ref<number | null>(null);

    // Getters
    const isEmpty = computed(() => screenshots.value.length === 0);

    // Actions
    function setScreenshots(items: Screenshot[]) {
        screenshots.value = items;
        if (items.length > 0) {
            lastId.value = items[items.length - 1].id;
        }
    }

    function appendScreenshots(items: Screenshot[]) {
        screenshots.value.push(...items);
        if (items.length > 0) {
            lastId.value = items[items.length - 1].id;
        }
    }

    function setCurrentScreenshot(screenshot: Screenshot | null) {
        currentScreenshot.value = screenshot;
    }

    function setLoading(status: boolean) {
        loading.value = status;
    }

    function setHasMore(status: boolean) {
        hasMore.value = status;
    }

    function removeScreenshot(id: number) {
        screenshots.value = screenshots.value.filter(s => s.id !== id);
        if (currentScreenshot.value?.id === id) {
            currentScreenshot.value = null;
        }
    }

    function reset() {
        screenshots.value = [];
        currentScreenshot.value = null;
        loading.value = false;
        hasMore.value = true;
        lastId.value = null;
    }

    return {
        // State
        screenshots,
        currentScreenshot,
        loading,
        hasMore,
        lastId,
        
        // Getters
        isEmpty,
        
        // Actions
        setScreenshots,
        appendScreenshots,
        setCurrentScreenshot,
        setLoading,
        setHasMore,
        removeScreenshot,
        reset
    };
}); 