<script setup lang="ts">
import { onMounted } from 'vue';
import { useRouter } from 'vue-router';
import { useMessage } from 'naive-ui';
import type { MonthStats } from '@/types/screenshot';
import MonthCard from '@/components/screenshot/MonthCard.vue';
import { useScreenshotCalendarStore } from '@/stores/screenshot-calendar';

const router = useRouter();
const message = useMessage();
const calendarStore = useScreenshotCalendarStore();

// 加载月份统计数据
async function loadMonthStats() {
  await calendarStore.loadMonthStats();
  if (calendarStore.error) {
    message.error(calendarStore.error);
  }
}

// 导航到月份视图
function navigateToMonth(month: MonthStats) {
  router.push({
    name: 'MonthView',
    params: {
      year: month.year.toString(),
      month: month.month.toString().padStart(2, '0')
    }
  });
}

onMounted(() => {
  loadMonthStats();
});
</script>

<template>
  <div class="calendar-view">
    <div class="header">
      <h1>Photo Calendar</h1>
    </div>
    
    <div v-if="calendarStore.loading" class="loading-state">
      Loading calendar...
    </div>
    
    <div v-else class="month-grid">
      <month-card
        v-for="month in calendarStore.months"
        :key="`${month.year}-${month.month}`"
        :stats="month"
        @click="navigateToMonth(month)"
      />
    </div>
  </div>
</template>

<style scoped>
.calendar-view {
  padding: 24px;
  max-width: 1400px;
  margin: 0 auto;
}

.header {
  margin-bottom: 24px;
}

.header h1 {
  font-size: 2em;
  font-weight: 600;
  color: #333;
  margin: 0;
}

.month-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
  gap: 24px;
}

.loading-state {
  text-align: center;
  padding: 48px;
  color: #666;
  font-size: 1.1em;
}
</style> 
