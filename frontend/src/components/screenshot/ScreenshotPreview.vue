<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, watch } from 'vue';
import { NIcon, NSpin } from 'naive-ui';
import { CloseOutline, ChevronBackOutline, ChevronForwardOutline } from '@vicons/ionicons5';
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
const previousIndex = ref(currentIndex.value);

watch(() => props.initialIndex, (newValue) => {
  if (newValue !== undefined) {
    previousIndex.value = currentIndex.value;
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
    previousIndex.value = currentIndex.value;
    currentIndex.value--;
    emit('update:currentIndex', currentIndex.value);
  }
};

const showNext = () => {
  if (currentIndex.value < props.screenshots.length - 1) {
    previousIndex.value = currentIndex.value;
    currentIndex.value++;
    emit('update:currentIndex', currentIndex.value);
  }
};

// 关闭预览时重置缩放
const closePreview = () => {
  emit('update:modelValue', false);
  resetZoom();
};

// 切换图片时重置缩放
watch(currentIndex, () => {
  resetZoom();
});

// 图片加载状态接口
interface ImageLoadingState {
  loading: boolean;
  error: boolean;
}

// 添加加载状态管理
const loadingStates = ref<Map<string, ImageLoadingState>>(new Map());

// 预加载函数
const preloadImages = (screenshots: Screenshot[], currentIndex: number) => {
  const indicesToLoad = [currentIndex];
  
  // 添加前两张
  for (let i = 1; i <= 2; i++) {
    if (currentIndex - i >= 0) {
      indicesToLoad.push(currentIndex - i);
    }
  }
  
  // 添加后两张
  for (let i = 1; i <= 3; i++) {
    if (currentIndex + i < screenshots.length) {
      indicesToLoad.push(currentIndex + i);
    }
  }
  
  indicesToLoad.forEach(index => {
    const screenshot = screenshots[index];
    if (!screenshot) return;
    
    const imageUrl = `/api/screenshots/${screenshot.id}/raw`;
    
    // 如果已经在加载或已加载，跳过
    if (loadingStates.value.has(imageUrl)) return;
    
    // 设置初始加载状态
    loadingStates.value.set(imageUrl, { loading: true, error: false });
    
    const img = new Image();
    img.onload = () => {
      loadingStates.value.set(imageUrl, { loading: false, error: false });
    };
    img.onerror = () => {
      loadingStates.value.set(imageUrl, { loading: false, error: true });
    };
    img.src = imageUrl;
  });
};

// 获取当前图片的加载状态
const currentImageState = computed(() => {
  if (!currentScreenshot.value) return { loading: false, error: false };
  const imageUrl = `/api/screenshots/${currentScreenshot.value.id}/raw`;
  return loadingStates.value.get(imageUrl) || { loading: true, error: false };
});

// 监听图片切换，触发预加载
watch(currentIndex, (newIndex) => {
  resetZoom();
  if (props.screenshots.length > 0) {
    preloadImages(props.screenshots, newIndex);
  }
});

// 初始预加载
onMounted(() => {
  window.addEventListener('keydown', handleKeyDown);
  window.addEventListener('mousemove', handleDrag);
  window.addEventListener('mouseup', handleDragEnd);
  
  if (props.screenshots.length > 0) {
    preloadImages(props.screenshots, currentIndex.value);
  }
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
      class="preview-backdrop"
      @click="closePreview"
      @wheel.prevent="handleWheel"
    >
      <div class="preview-content">
        <!-- 关闭按钮 -->
        <button class="control-button close-button" @click="closePreview">
          <n-icon size="24">
            <CloseOutline />
          </n-icon>
        </button>

        <!-- 导航按钮 -->
        <button 
          class="control-button nav-button prev" 
          @click.stop="showPrevious"
          v-show="currentIndex > 0"
        >
          <n-icon size="32">
            <ChevronBackOutline />
          </n-icon>
        </button>
        <button 
          class="control-button nav-button next" 
          @click.stop="showNext"
          v-show="currentIndex < screenshots.length - 1"
        >
          <n-icon size="32">
            <ChevronForwardOutline />
          </n-icon>
        </button>

        <!-- 图片容器 -->
        <div class="image-wrapper">
          <Transition name="slide" mode="out-in">
            <div 
              :key="currentScreenshot?.id"
              class="image-container"
              :class="{ 
                'dragging': isDragging,
                'sliding-prev': currentIndex < previousIndex,
                'sliding-next': currentIndex > previousIndex
              }"
              :style="{
                transform: `translate3d(0,0,0) scale(${scale}) translate(${position.x}px, ${position.y}px)`,
                cursor: scale > 1 ? (isDragging ? 'grabbing' : 'grab') : 'default'
              }"
              @mousedown.stop="handleDragStart"
            >
              <!-- 加载状态 -->
              <div v-if="currentImageState.loading" class="loading-overlay">
                <n-spin size="large" />
              </div>
              
              <!-- 错误状态 -->
              <div v-else-if="currentImageState.error" class="error-overlay">
                <div class="error-message">
                  图片加载失败
                </div>
              </div>
              
              <!-- 图片 -->
              <img 
                ref="imageRef"
                :src="`/api/screenshots/${currentScreenshot?.id}/raw`"
                :alt="currentScreenshot?.filename"
                class="preview-image"
                :class="{ 'image-loading': currentImageState.loading }"
                @click.stop
              />
            </div>
          </Transition>
        </div>
      </div>
    </div>
  </Transition>
</template>

<style scoped>
.preview-backdrop {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: rgba(0, 0, 0, 0.9);
  z-index: 1000;
  display: flex;
  align-items: center;
  justify-content: center;
}

.preview-content {
  position: relative;
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
}

.image-wrapper {
  position: relative;
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
}

.image-container {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  transform-origin: center center;
  width: 100%;
  height: 100%;
  will-change: transform;
}

.preview-image {
  max-width: 100%;
  max-height: 100%;
  object-fit: contain;
  user-select: none;
  transform-origin: center center;
  will-change: transform;
  touch-action: none;
  backface-visibility: hidden;
  -webkit-backface-visibility: hidden;
}

.preview-image:not(.dragging) {
  transition: transform 0.1s ease-out;
}

.control-button {
  position: absolute;
  border: none;
  background: transparent;
  color: rgba(255, 255, 255, 0.45);
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: color 0.3s;
  z-index: 1001;
  padding: 8px;
}

.control-button:hover {
  color: rgba(255, 255, 255, 0.9);
}

.close-button {
  top: 16px;
  right: 16px;
}

.nav-button {
  top: 50%;
  transform: translateY(-50%);
}

.prev {
  left: 16px;
}

.next {
  right: 16px;
}

/* 过渡动画 */
.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.3s ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}

.slide-enter-active,
.slide-leave-active {
  transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
  position: absolute;
  width: 100%;
  height: 100%;
}

.slide-enter-from {
  opacity: 0;
  transform: translateX(30px);
}

.slide-leave-to {
  opacity: 0;
  transform: translateX(-30px);
}

.loading-overlay,
.error-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  background: rgba(0, 0, 0, 0.5);
  z-index: 1;
}

.error-message {
  color: #fff;
  background: rgba(255, 71, 87, 0.8);
  padding: 8px 16px;
  border-radius: 4px;
  font-size: 14px;
}

.image-loading {
  opacity: 0.5;
  transition: opacity 0.3s ease;
}
</style> 