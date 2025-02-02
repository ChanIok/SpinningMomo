import { defineStore } from 'pinia';
import { ref, computed } from 'vue';
import type { Screenshot } from '@/types/screenshot';
import { screenshotAPI } from '@/api/screenshot';

export const useScreenshotListStore = defineStore('screenshotList', () => {
    // State
    const screenshots = ref<Screenshot[]>([]);
    const loading = ref(false);
    const hasMore = ref(true);
    const lastId = ref<number | undefined>(undefined);
    const error = ref<string | null>(null);
    
    // 配置
    const batchSize = 50;

    // Getters
    const isEmpty = computed(() => screenshots.value.length === 0);

    // Actions
    async function loadMore() {
        if (loading.value || !hasMore.value) return;
        console.log('Loading more screenshots-store');
        loading.value = true;
        error.value = null;

        try {
            const response = await screenshotAPI.getScreenshots({
                lastId: lastId.value,
                limit: batchSize
            });
            
            if (response.screenshots.length > 0) {
                screenshots.value.push(...response.screenshots);
                lastId.value = response.screenshots[response.screenshots.length - 1].id;
            }

            hasMore.value = response.hasMore;
        } catch (e) {
            error.value = e instanceof Error ? e.message : 'Failed to load screenshots';
            console.error('Failed to load screenshots:', e);
        } finally {
            loading.value = false;
        }
    }

    async function loadByMonth(year: number, month: number, reset = false) {
        if (loading.value || (!hasMore.value && !reset)) return;

        if (reset) {
            screenshots.value = [];
            lastId.value = undefined;
            hasMore.value = true;
        }

        loading.value = true;
        error.value = null;

        try {
            const response = await screenshotAPI.getScreenshotsByMonth({
                year,
                month,
                lastId: lastId.value,
                limit: batchSize
            });

            if (response.screenshots.length > 0) {
                screenshots.value.push(...response.screenshots);
                lastId.value = response.screenshots[response.screenshots.length - 1].id;
            }

            hasMore.value = response.hasMore;
        } catch (e) {
            error.value = e instanceof Error ? e.message : 'Failed to load screenshots';
            console.error('Failed to load screenshots:', e);
        } finally {
            loading.value = false;
        }
    }

    function reset() {
        screenshots.value = [];
        loading.value = false;
        hasMore.value = true;
        lastId.value = undefined;
        error.value = null;
    }

    return {
        // State
        screenshots,
        loading,
        hasMore,
        error,
        
        // Getters
        isEmpty,
        
        // Actions
        loadMore,
        loadByMonth,
        reset
    };
}); 