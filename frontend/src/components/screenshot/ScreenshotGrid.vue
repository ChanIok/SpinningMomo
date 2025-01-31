<template>
  <div class="screenshot-grid-container">
    <n-grid
      :x-gap="16"
      :y-gap="16"
      :cols="cols"
    >
      <n-grid-item v-for="screenshot in screenshots" :key="screenshot.id">
        <screenshot-card
          :screenshot="screenshot"
          :selected="selectedId === screenshot.id"
          @click="handleScreenshotClick"
        />
      </n-grid-item>
    </n-grid>

    <div v-if="loading" class="loading-container">
      <n-spin size="large" />
    </div>

    <div v-if="!loading && screenshots.length === 0" class="empty-container">
      <n-empty description="No screenshots found" />
    </div>

    <div v-if="!loading && hasMore" class="load-more">
      <n-button @click="$emit('load-more')" :loading="loading">
        Load More
      </n-button>
    </div>

    <div ref="loadingRef" class="loading-trigger">
      <n-spin v-if="loading" />
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed } from 'vue';
import { NGrid, NGridItem, NSpin, NEmpty, NButton } from 'naive-ui';
import type { Screenshot } from '@/types/screenshot';
import ScreenshotCard from './ScreenshotCard.vue';
import { useScreenshotStore } from '@/stores/screenshot';
import { useIntersectionObserver } from '@vueuse/core';

const props = defineProps<{
  screenshots: Screenshot[];
  loading?: boolean;
  selectedId?: number;
  hasMore?: boolean;
}>();

const emit = defineEmits<{
  (e: 'screenshot-click', screenshot: Screenshot): void;
  (e: 'load-more'): void;
}>();

const store = useScreenshotStore();
const loadingRef = ref<HTMLElement | null>(null);
const loading = ref(false);

const screenshots = computed(() => store.screenshots);

// 响应式网格列数
const cols = ref(4);

// 屏幕断点配置
const breakPoints = {
  xs: 1,
  s: 2,
  m: 3,
  l: 4,
  xl: 5,
  '2xl': 6
};

// 根据屏幕宽度更新列数
function updateCols() {
  const width = window.innerWidth;
  if (width < 640) cols.value = breakPoints.xs;
  else if (width < 768) cols.value = breakPoints.s;
  else if (width < 1024) cols.value = breakPoints.m;
  else if (width < 1280) cols.value = breakPoints.l;
  else if (width < 1536) cols.value = breakPoints.xl;
  else cols.value = breakPoints['2xl'];
}

// 初始化并添加 resize 监听器
onMounted(() => {
  updateCols();
  window.addEventListener('resize', updateCols);
});

onUnmounted(() => {
  window.removeEventListener('resize', updateCols);
});

// 使用 Intersection Observer 实现无限滚动
const { stop } = useIntersectionObserver(
  loadingRef,
  async ([{ isIntersecting }]) => {
    if (isIntersecting && !loading.value && !store.reachedEnd) {
      loading.value = true;
      await store.loadMoreScreenshots();
      loading.value = false;
    }
  }
);

onUnmounted(() => {
  stop();
});

function handleScreenshotClick(screenshot: Screenshot) {
  emit('screenshot-click', screenshot);
}
</script>

<style scoped>
.screenshot-grid-container {
  width: 100%;
  min-height: 200px;
  position: relative;
  padding: 16px;
}

.loading-container {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
}

.empty-container {
  padding: 48px 0;
  display: flex;
  justify-content: center;
}

.load-more {
  padding: 24px 0;
  display: flex;
  justify-content: center;
}

.loading-trigger {
  height: 50px;
  display: flex;
  align-items: center;
  justify-content: center;
}
</style> 