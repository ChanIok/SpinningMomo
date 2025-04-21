<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, watch } from 'vue';
import { CloseOutline, ChevronBackOutline, ChevronForwardOutline, ReloadOutline } from '@vicons/ionicons5';
import type { Screenshot } from '@/types/screenshot';

const props = defineProps<{
  modelValue: boolean;
  screenshots: Screenshot[];
  initialIndex?: number;
}>();

const emit = defineEmits<{
  'update:modelValue': [value: boolean];
  'update:currentIndex': [index: number];
}>();

const isVisible = computed({
  get: () => props.modelValue,
  set: (value) => emit('update:modelValue', value)
});

// 添加缩放和拖动状态
const scale = ref(1);
const MIN_SCALE = 0.5;
const MAX_SCALE = 5;
const position = ref({ x: 0, y: 0 });
const isDragging = ref(false);
const dragStart = ref({ x: 0, y: 0 });
const imageRef = ref<HTMLImageElement | null>(null);
let rafId: number | null = null;

// 使用 requestAnimationFrame 更新位置
const updatePosition = (x: number, y: number) => {
  if (rafId) {
    cancelAnimationFrame(rafId);
  }
  rafId = requestAnimationFrame(() => {
    position.value = { x, y };
    rafId = null;
  });
};

// 计算边界限制
const getBoundaries = () => {
  if (!imageRef.value) return { minX: 0, maxX: 0, minY: 0, maxY: 0 };

  const img = imageRef.value;
  const container = img.parentElement;
  if (!container) return { minX: 0, maxX: 0, minY: 0, maxY: 0 };

  const containerRect = container.getBoundingClientRect();
  const imgRect = img.getBoundingClientRect();

  // 计算缩放后的尺寸
  const scaledWidth = imgRect.width * scale.value;
  const scaledHeight = imgRect.height * scale.value;

  // 计算边界，确保图片边缘始终在容器内
  // 如果缩放后的尺寸大于容器，则限制边缘不能超出容器
  // 如果缩放后的尺寸小于容器，则限制图片居中
  const horizontalExcess = Math.max(0, (scaledWidth - containerRect.width) / 2);
  const verticalExcess = Math.max(0, (scaledHeight - containerRect.height) / 2);

  return {
    minX: -horizontalExcess,
    maxX: horizontalExcess,
    minY: -verticalExcess,
    maxY: verticalExcess
  };
};

// 限制值在范围内
const clamp = (value: number, min: number, max: number) => {
  return Math.min(Math.max(value, min), max);
};

// 处理滚轮缩放
const handleWheel = (e: WheelEvent) => {
  if (!isVisible.value) return;
  e.preventDefault();

  // 增加缩放步进值
  const delta = e.deltaY > 0 ? -0.35 : 0.35;
  const newScale = Math.min(Math.max(scale.value + delta, MIN_SCALE), MAX_SCALE);

  // 如果缩小到1以下，重置位置
  if (newScale <= 1 && scale.value > 1) {
    position.value = { x: 0, y: 0 };
  }

  scale.value = newScale;

  // 更新位置以确保在边界内
  if (scale.value > 1) {
    const boundaries = getBoundaries();
    position.value = {
      x: clamp(position.value.x, boundaries.minX, boundaries.maxX),
      y: clamp(position.value.y, boundaries.minY, boundaries.maxY)
    };
  }
};

// 处理拖动
const handleDrag = (e: MouseEvent) => {
  if (!isDragging.value || scale.value <= 1) return;
  e.preventDefault();

  const boundaries = getBoundaries();

  // 直接使用鼠标位置和起始位置的差值计算新位置
  const deltaX = e.clientX - dragStart.value.x;
  const deltaY = e.clientY - dragStart.value.y;

  const newX = clamp(
    deltaX / scale.value,
    boundaries.minX,
    boundaries.maxX
  );
  const newY = clamp(
    deltaY / scale.value,
    boundaries.minY,
    boundaries.maxY
  );

  updatePosition(newX, newY);
};

// 处理拖动开始
const handleDragStart = (e: MouseEvent) => {
  if (scale.value <= 1) return;
  isDragging.value = true;
  dragStart.value = {
    x: e.clientX - position.value.x * scale.value,
    y: e.clientY - position.value.y * scale.value
  };
};

// 处理拖动结束
const handleDragEnd = () => {
  isDragging.value = false;
  if (rafId) {
    cancelAnimationFrame(rafId);
    rafId = null;
  }
};

// 重置缩放和位置
const resetZoom = () => {
  scale.value = 1;
  position.value = { x: 0, y: 0 };
};

const currentIndex = ref(props.initialIndex ?? 0);

watch(() => props.initialIndex, (newValue) => {
  if (newValue !== undefined) {
    currentIndex.value = newValue;
  }
});

// 处理键盘事件
const handleKeyDown = (e: KeyboardEvent) => {
  if (!isVisible.value) return;

  if (e.key === 'ArrowLeft') {
    showPrevious();
  } else if (e.key === 'ArrowRight') {
    showNext();
  } else if (e.key === 'Escape') {
    closePreview();
  }
};

const showPrevious = () => {
  if (currentIndex.value > 0) {
    currentIndex.value--;
    emit('update:currentIndex', currentIndex.value);
  }
};

const showNext = () => {
  if (currentIndex.value < props.screenshots.length - 1) {
    currentIndex.value++;
    emit('update:currentIndex', currentIndex.value);
  }
};

// 关闭预览时重置缩放
const closePreview = () => {
  // 使用currentScreenshot确保它被读取，避免警告
  if (currentScreenshot.value) {
    console.log(`关闭预览，当前截图 ID: ${currentScreenshot.value.id}`);
  }
  emit('update:modelValue', false);
  resetZoom();
};

// 切换图片时重置缩放
watch(currentIndex, () => {
  resetZoom();
});

// 获取当前截图 - 保留以便在其他地方使用
const currentScreenshot = computed(() => {
  return props.screenshots[currentIndex.value];
});

// 删除不需要的计算属性

// 预渲染池相关变量
const BUFFER_SIZE = 5; // 预渲染前后各两张，加上当前图片共五张
const firstVisibleIndex = computed(() => Math.max(0, currentIndex.value - Math.floor(BUFFER_SIZE / 2)));
const visibleScreenshots = computed(() => {
  const start = firstVisibleIndex.value;
  const end = Math.min(props.screenshots.length, start + BUFFER_SIZE);
  return props.screenshots.slice(start, end);
});

// 图片引用集合
const imageRefs = ref<Map<number, HTMLElement>>(
  new Map()
);

// 设置图片引用
const setImageRef = (el: any, screenshot: Screenshot) => {
  if (el && el instanceof HTMLElement) {
    imageRefs.value.set(screenshot.id, el);
  }
};

// 获取图片状态
const getImageState = (id: number) => {
  const imageUrl = `/api/screenshots/${id}/raw`;
  // 如果状态不存在，初始化为加载中
  if (!loadingStates.value.has(imageUrl)) {
    loadingStates.value.set(imageUrl, { loading: true, error: false });
  }
  return loadingStates.value.get(imageUrl) || { loading: true, error: false };
};

// 图片加载状态接口
interface ImageLoadingState {
  loading: boolean;
  error: boolean;
}

// 添加加载状态管理
const loadingStates = ref<Map<string, ImageLoadingState>>(new Map());

// 图片加载事件处理
const handleImageLoad = (id: number) => {
  const imageUrl = `/api/screenshots/${id}/raw`;
  loadingStates.value.set(imageUrl, { loading: false, error: false });
};

const handleImageError = (id: number) => {
  const imageUrl = `/api/screenshots/${id}/raw`;
  loadingStates.value.set(imageUrl, { loading: false, error: true });
};

// 删除不再使用的函数

// 监听图片切换，重置缩放
watch(currentIndex, () => {
  resetZoom();
});

// 添加事件监听
onMounted(() => {
  window.addEventListener('keydown', handleKeyDown);
  window.addEventListener('mousemove', handleDrag);
  window.addEventListener('mouseup', handleDragEnd);
});

onUnmounted(() => {
  window.removeEventListener('keydown', handleKeyDown);
  window.removeEventListener('mousemove', handleDrag);
  window.removeEventListener('mouseup', handleDragEnd);
  if (rafId) {
    cancelAnimationFrame(rafId);
  }
});
</script>

<template>
  <Transition name="fade">
    <div
      v-if="isVisible"
      class="fixed inset-0 bg-black/90 z-[1000] flex items-center justify-center"
      @click="closePreview"
      @wheel.prevent="handleWheel"
    >
      <div class="relative w-full h-full flex items-center justify-center">
        <!-- 关闭按钮 -->
        <button class="absolute top-4 right-4 p-2 text-white/45 hover:text-white/90 transition-colors z-[1001] flex items-center justify-center" @click="closePreview">
          <component :is="CloseOutline" class="w-6 h-6" />
        </button>

        <!-- 导航按钮 -->
        <button
          class="absolute top-1/2 left-4 -translate-y-1/2 p-2 text-white/45 hover:text-white/90 transition-colors z-[1001] flex items-center justify-center"
          @click.stop="showPrevious"
          v-show="currentIndex > 0"
        >
          <component :is="ChevronBackOutline" class="w-8 h-8" />
        </button>
        <button
          class="absolute top-1/2 right-4 -translate-y-1/2 p-2 text-white/45 hover:text-white/90 transition-colors z-[1001] flex items-center justify-center"
          @click.stop="showNext"
          v-show="currentIndex < screenshots.length - 1"
        >
          <component :is="ChevronForwardOutline" class="w-8 h-8" />
        </button>

        <!-- 图片容器 - 预渲染池 -->
        <div class="relative w-full h-full flex items-center justify-center">
          <!-- 预渲染图片池 -->
          <div
            v-for="(screenshot, index) in visibleScreenshots"
            :key="screenshot.id"
            class="absolute inset-0 flex items-center justify-center transition-opacity duration-100"
            :class="{ 'opacity-0 pointer-events-none': index !== (currentIndex - firstVisibleIndex) }"
            :style="{ 'z-index': index === (currentIndex - firstVisibleIndex) ? 1 : 0 }"
          >
            <div
              class="relative flex items-center justify-center w-full h-full will-change-transform origin-center"
              :class="{
                'dragging': isDragging
              }"
              :style="{
                transform: `translate3d(0,0,0) scale(${scale}) translate(${position.x}px, ${position.y}px)`,
                cursor: scale > 1 ? (isDragging ? 'grabbing' : 'grab') : 'default'
              }"
              @mousedown.stop="handleDragStart"
            >
              <!-- 加载状态 -->
              <div v-if="getImageState(screenshot.id).loading" class="absolute inset-0 flex items-center justify-center bg-black/30">
                <component :is="ReloadOutline" class="w-12 h-12 text-white animate-spin" />
              </div>

              <!-- 错误状态 -->
              <div v-else-if="getImageState(screenshot.id).error" class="absolute inset-0 flex items-center justify-center bg-black/30">
                <div class="px-6 py-4 bg-red-500/80 text-white rounded-lg text-center">
                  图片加载失败
                </div>
              </div>

              <!-- 图片 -->
              <img
                :ref="el => setImageRef(el, screenshot)"
                :src="`/api/screenshots/${screenshot.id}/raw`"
                :alt="screenshot.filename"
                class="max-w-full max-h-full object-contain select-none origin-center will-change-transform touch-none backface-hidden"
                :class="{ 'opacity-50': getImageState(screenshot.id).loading }"
                @click.stop
                @load="handleImageLoad(screenshot.id)"
                @error="handleImageError(screenshot.id)"
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  </Transition>
</template>

<style scoped>
/* 保留背景渐变动画 */
.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.3s ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}

/* 添加一些工具类，补充Tailwind不直接支持的属性 */
.backface-hidden {
  backface-visibility: hidden;
  -webkit-backface-visibility: hidden;
}

.dragging img {
  transition: none !important;
}

img:not(.dragging) {
  transition: transform 0.1s ease-out;
}
</style>