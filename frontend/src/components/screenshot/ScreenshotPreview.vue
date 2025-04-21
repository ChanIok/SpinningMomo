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
    loadingStates.value.set(imageUrl, { loading: true, error: false, thumbnailLoaded: false });
  }
  return loadingStates.value.get(imageUrl) || { loading: true, error: false, thumbnailLoaded: false };
};

// 图片加载状态接口
interface ImageLoadingState {
  loading: boolean;
  error: boolean;
  thumbnailLoaded: boolean; // 缩略图是否已加载
}

// 添加加载状态管理
const loadingStates = ref<Map<string, ImageLoadingState>>(new Map());

// 图片加载队列管理
interface QueueItem {
  id: number;
  priority: number;
  type: 'raw' | 'thumbnail';
}

const loadQueue = ref<QueueItem[]>([]);
const activeLoads = ref(0);
const MAX_CONCURRENT_LOADS = 2; // 限制并发加载数

// 存储每个图片请求对应的AbortController
const abortControllers = ref(new Map<string, AbortController>());

// 处理队列
const processQueue = () => {
  if (loadQueue.value.length === 0 || activeLoads.value >= MAX_CONCURRENT_LOADS) {
    return;
  }

  // 获取队列中优先级最高的项
  loadQueue.value.sort((a, b) => b.priority - a.priority);
  const { id, type } = loadQueue.value.shift()!;

  // 开始加载
  activeLoads.value++;

  // 创建AbortController
  const controller = new AbortController();
  const imageUrl = `/api/screenshots/${id}/${type}`;
  const key = `${id}-${type}`;

  // 如果已有相同请求，取消它
  if (abortControllers.value.has(key)) {
    abortControllers.value.get(key)?.abort();
  }

  // 存储新的controller
  abortControllers.value.set(key, controller);

  // 使用fetch代替Image对象加载
  fetch(imageUrl, { signal: controller.signal })
    .then(response => {
      if (!response.ok) throw new Error('Network response was not ok');
      return response.blob();
    })
    .then(blob => {
      // 创建blob URL
      const url = URL.createObjectURL(blob);

      // 使用Image对象加载blob URL以触发onload事件
      const img = new Image();
      img.onload = () => {
        // 更新状态
        if (type === 'raw') {
          loadingStates.value.set(imageUrl, {
            loading: false,
            error: false,
            thumbnailLoaded: true
          });
        } else if (type === 'thumbnail') {
          const rawUrl = `/api/screenshots/${id}/raw`;
          const currentState = loadingStates.value.get(rawUrl) || { loading: true, error: false, thumbnailLoaded: false };
          loadingStates.value.set(rawUrl, { ...currentState, thumbnailLoaded: true });
        }

        // 清理
        URL.revokeObjectURL(url);
        abortControllers.value.delete(key);
        activeLoads.value--;

        // 处理下一个
        processQueue();
      };

      img.src = url;
    })
    .catch(err => {
      // 如果是取消请求导致的错误，不做特殊处理
      if (err.name === 'AbortError') {
        console.log(`Request for ${imageUrl} was aborted`);
      } else {
        // 其他错误
        if (type === 'raw') {
          loadingStates.value.set(imageUrl, {
            loading: false,
            error: true,
            thumbnailLoaded: true
          });
        } else if (type === 'thumbnail') {
          const rawUrl = `/api/screenshots/${id}/raw`;
          const currentState = loadingStates.value.get(rawUrl) || { loading: true, error: false, thumbnailLoaded: false };
          loadingStates.value.set(rawUrl, { ...currentState, thumbnailLoaded: true, error: true });
        }
      }

      // 清理
      abortControllers.value.delete(key);
      activeLoads.value--;

      // 处理下一个
      processQueue();
    });
};

// 添加到队列
const queueImageLoad = (id: number, type: 'raw' | 'thumbnail', priority: number) => {
  // 如果已在队列中，更新优先级
  const existingIndex = loadQueue.value.findIndex(item => item.id === id && item.type === type);
  if (existingIndex >= 0) {
    loadQueue.value[existingIndex].priority = Math.max(loadQueue.value[existingIndex].priority, priority);
    return;
  }

  // 添加到队列
  loadQueue.value.push({ id, type, priority });
  processQueue();
};

// 加载图片的方法
const loadScreenshotImage = (screenshot: Screenshot, isVisible: boolean) => {
  const id = screenshot.id;
  const imageUrl = `/api/screenshots/${id}/raw`;

  // 初始化加载状态
  if (!loadingStates.value.has(imageUrl)) {
    loadingStates.value.set(imageUrl, { loading: true, error: false, thumbnailLoaded: false });
  }

  // 设置优先级
  const thumbnailPriority = isVisible ? 20 : 15; // 缩略图始终有更高的优先级
  const rawPriority = isVisible ? 10 : 5;

  // 先加载缩略图，再加载原图
  queueImageLoad(id, 'thumbnail', thumbnailPriority);
  queueImageLoad(id, 'raw', rawPriority);
};

// 已移除事件处理函数，改为使用队列管理

// 删除不再使用的函数

// 监听图片切换，重置缩放并更新加载队列
watch(currentIndex, (newIndex) => {
  resetZoom();

  // 清空队列中不在预渲染池的项
  loadQueue.value = loadQueue.value.filter(item => {
    const index = props.screenshots.findIndex(s => s.id === item.id);
    return Math.abs(index - newIndex) <= Math.floor(BUFFER_SIZE / 2);
  });

  // 取消不在预渲染池的请求
  abortControllers.value.forEach((controller, key) => {
    const [idStr] = key.split('-');
    const id = parseInt(idStr);
    const index = props.screenshots.findIndex(s => s.id === id);
    const isInRenderPool = Math.abs(index - newIndex) <= Math.floor(BUFFER_SIZE / 2);

    if (!isInRenderPool) {
      controller.abort();
      abortControllers.value.delete(key);
    }
  });

  // 为可见图片设置高优先级
  visibleScreenshots.value.forEach((screenshot, index) => {
    const isVisible = index === (newIndex - firstVisibleIndex.value);
    loadScreenshotImage(screenshot, isVisible);
  });
});

// 添加事件监听
onMounted(() => {
  window.addEventListener('keydown', handleKeyDown);
  window.addEventListener('mousemove', handleDrag);
  window.addEventListener('mouseup', handleDragEnd);

  // 初始加载当前可见图片
  if (props.screenshots.length > 0) {
    visibleScreenshots.value.forEach((screenshot, index) => {
      const isVisible = index === (currentIndex.value - firstVisibleIndex.value);
      loadScreenshotImage(screenshot, isVisible);
    });
  }
});

onUnmounted(() => {
  window.removeEventListener('keydown', handleKeyDown);
  window.removeEventListener('mousemove', handleDrag);
  window.removeEventListener('mouseup', handleDragEnd);
  if (rafId) {
    cancelAnimationFrame(rafId);
  }

  // 取消所有请求
  abortControllers.value.forEach(controller => {
    controller.abort();
  });
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
              <!-- 缩略图（占位） -->
              <div class="absolute inset-0 flex items-center justify-center">
                <img
                  :src="`/api/screenshots/${screenshot.id}/thumbnail`"
                  :alt="screenshot.filename"
                  class="w-full h-full object-contain select-none transition-all duration-300"
                  :class="{ 'opacity-0': !getImageState(screenshot.id).loading }"
                  @click.stop
                />
              </div>

              <!-- 高质量原图 -->
              <div class="absolute inset-0 flex items-center justify-center">
                <img
                  :ref="el => setImageRef(el, screenshot)"
                  :src="`/api/screenshots/${screenshot.id}/raw`"
                  :alt="screenshot.filename"
                  class="max-w-full max-h-full object-contain select-none origin-center will-change-transform touch-none backface-hidden transition-opacity duration-300"
                  :class="{ 'opacity-0': getImageState(screenshot.id).loading }"
                  @click.stop
                />
              </div>

              <!-- 错误状态 -->
              <div v-if="getImageState(screenshot.id).error" class="absolute inset-0 flex items-center justify-center bg-black/30">
                <div class="px-6 py-4 bg-red-500/80 text-white rounded-lg text-center">
                  图片加载失败
                </div>
              </div>

              <!-- 加载状态（当缩略图也未加载时显示） -->
              <div v-if="getImageState(screenshot.id).loading && !getImageState(screenshot.id).thumbnailLoaded" class="absolute inset-0 flex items-center justify-center bg-black/30">
                <component :is="ReloadOutline" class="w-12 h-12 text-white animate-spin" />
              </div>
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
  transition: transform 0.1s ease-out, opacity 0.3s ease;
}

/* 缩略图到原图的过渡效果 */
.blur-sm.opacity-0 {
  filter: blur(0);
  transform: scale(1);
}
</style>