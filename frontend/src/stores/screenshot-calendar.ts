import { defineStore } from 'pinia';
import { ref } from 'vue';
import type { MonthStats } from '@/types/screenshot';
import { screenshotAPI } from '@/api/screenshot';

export const useScreenshotCalendarStore = defineStore('screenshotCalendar', () => {
    // State
    const months = ref<MonthStats[]>([]);
    const loading = ref(false);
    const error = ref<string | null>(null);

    // Actions
    async function loadMonthStats() {
        if (loading.value) return;

        loading.value = true;
        error.value = null;

        try {
            const stats = await screenshotAPI.getMonthStatistics();
            months.value = stats;
        } catch (e) {
            error.value = e instanceof Error ? e.message : 'Failed to load calendar data';
            console.error('Failed to load month statistics:', e);
        } finally {
            loading.value = false;
        }
    }

    function reset() {
        months.value = [];
        loading.value = false;
        error.value = null;
    }

    return {
        // State
        months,
        loading,
        error,

        // Actions
        loadMonthStats,
        reset
    };
}); 