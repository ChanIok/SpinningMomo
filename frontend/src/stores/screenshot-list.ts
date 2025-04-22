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
    const currentIndex = ref<number>(-1); // 新增：当前选中的截图索引

    // Getters
    const isEmpty = computed(() => screenshots.value.length === 0);

    // 新增：当前选中的截图
    const currentScreenshot = computed(() => {
        if (currentIndex.value >= 0 && currentIndex.value < screenshots.value.length) {
            return screenshots.value[currentIndex.value];
        }
        return null;
    });

    // 新增：当前选中的截图ID
    const currentScreenshotId = computed(() => {
        return currentScreenshot.value?.id;
    });

    // Actions
    async function loadScreenshots(params?: Partial<ScreenshotParams>) {
        if (loading.value || (!hasMore.value && lastId.value !== null)) return;

        try {
            loading.value = true;
            error.value = null;

            const result = await screenshotAPI.getScreenshots({
                lastId: lastId.value?.toString(),
                limit: 50,
                ...params
            });

            if (lastId.value === null) {
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
        currentIndex.value = -1; // 重置当前选中的截图索引
    }

    // 新增：设置当前选中的截图索引
    function setCurrentIndex(index: number) {
        if (index >= -1 && index < screenshots.value.length) {
            currentIndex.value = index;
        }
    }

    return {
        // State
        screenshots,
        loading,
        hasMore,
        lastId,
        error,
        currentIndex,

        // Getters
        isEmpty,
        currentScreenshot,
        currentScreenshotId,

        // Actions
        loadScreenshots,
        removeScreenshot,
        reset,
        setCurrentIndex
    };
});