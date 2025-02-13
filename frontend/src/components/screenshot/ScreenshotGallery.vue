<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import type { Screenshot } from '@/types/screenshot';
import { NImage, NEllipsis, NButton, NIcon, useMessage, NSpace, NTooltip } from 'naive-ui';
import { CheckmarkCircle, RadioButtonOff, Close, Albums } from '@vicons/ionicons5'
import ScreenshotPreview from './ScreenshotPreview.vue';
import { useSelectionStore } from '@/stores/screenshot-selection'
import AlbumSelectionDialog from '@/views/album/components/AlbumSelectionDialog.vue'
import { albumAPI } from '@/api/album'

// 布局配置常量
const BASE_HEIGHT = 320;        // 基准行高
const SPACING = 8;             // 图片间距（像素）
const VERTICAL_RATIO = 0.8;    // 垂直构图比例阈值（高/宽 > 此值认为是垂直构图）
const HORIZONTAL_RATIO = 1.2;  // 水平构图比例阈值（宽/高 > 此值认为是水平构图）

// 容器相关的响应式引用
const containerRef = ref<HTMLElement | null>(null);
const containerWidth = ref(0);

// 布局计算相关的类型定义
interface RowItem {
  screenshot: Screenshot;
  width: number;
  height: number;
}

interface Row {
  items: RowItem[];
  height: number;
}

const props = defineProps<{
  screenshots: Screenshot[];
  loading?: boolean;
  hasMore?: boolean;
}>();

const emit = defineEmits<{
  loadMore: []
}>();

// 预览相关的状态
const showPreview = ref(false);
const previewIndex = ref(0);

// 选择相关的状态
const selectionStore = useSelectionStore()
const showAlbumDialog = ref(false)
const message = useMessage()

// 处理照片点击
const handleImageClick = (index: number) => {
  const screenshot = props.screenshots[index]
  if (selectionStore.isSelectionMode) {
    selectionStore.toggleSelection(screenshot.id)
  } else {
    previewIndex.value = index
    showPreview.value = true
  }
}

// 处理添加到相册
async function handleAddToAlbum(albumId: number) {
  try {
    const selectedIds = Array.from(selectionStore.selectedIds)
    await albumAPI.addScreenshots(albumId, selectedIds)
    message.success('已成功添加到相册')
    selectionStore.exitSelectionMode()
  } catch (error) {
    console.error('Failed to add to album:', error)
    message.error('添加到相册失败')
  }
}

// 监听并更新容器宽度
const updateContainerWidth = () => {
  if (containerRef.value) {
    containerWidth.value = containerRef.value.offsetWidth;
  }
};

// 使用ResizeObserver监听容器尺寸变化
watch(() => containerRef.value, (el) => {
  if (el) {
    const observer = new ResizeObserver(() => {
      updateContainerWidth();
    });
    observer.observe(el);
    return () => observer.disconnect();
  }
}, { immediate: true });

// 计算单行布局
const calculateRowLayout = (screenshots: Screenshot[], containerWidth: number): Row => {
  // 计算总宽度和初始化每个项的尺寸
  let totalWidth = 0;
  const items: RowItem[] = screenshots.map(screenshot => {
    const aspectRatio = screenshot.width / screenshot.height;
    const height = BASE_HEIGHT;
    const width = aspectRatio * height;
    totalWidth += width;
    return {
      screenshot,
      width,
      height
    };
  });

  // 添加间距到总宽度
  totalWidth += (screenshots.length - 1) * SPACING;

  // 缩放以适应容器宽度
  const scale = (containerWidth - (screenshots.length - 1) * SPACING) / (totalWidth - (screenshots.length - 1) * SPACING);
  const rowHeight = BASE_HEIGHT * scale;

  // 调整每个项的尺寸
  items.forEach(item => {
    item.height = rowHeight;
    item.width = (item.width / BASE_HEIGHT) * rowHeight;
  });

  return {
    items,
    height: rowHeight
  };
};

// 计算完整布局
const layout = computed(() => {
  if (!containerWidth.value || !props.screenshots.length) {
    return {
      rows: [],
      totalHeight: 0
    };
  }

  const rows: Row[] = [];
  let currentRow: Screenshot[] = [];
  let currentRowWidth = 0;

  props.screenshots.forEach((screenshot, index) => {
    const aspectRatio = screenshot.width / screenshot.height;
    let baseHeight = BASE_HEIGHT;

    // 根据构图特征动态调整基准高度
    if (aspectRatio < VERTICAL_RATIO) {
      baseHeight = BASE_HEIGHT * 1.5;
    } else if (aspectRatio > HORIZONTAL_RATIO) {
      baseHeight = BASE_HEIGHT * 1;
    }

    // 计算预估宽度时使用调整后的高度
    const estimatedWidth = aspectRatio * baseHeight;
    
    currentRow.push(screenshot);
    currentRowWidth += estimatedWidth;

    // 添加图片间距
    if (currentRow.length > 1) {
      currentRowWidth += SPACING;
    }

    // 检查是否应该完成当前行
    const isLastImage = index === props.screenshots.length - 1;
    const wouldExceedWidth = currentRowWidth >= containerWidth.value;

    if (wouldExceedWidth || isLastImage) {
      // 计算行布局
      const row = calculateRowLayout(currentRow, containerWidth.value);
      
      // 特殊处理最后一行单张图片
      if (isLastImage && !wouldExceedWidth && currentRow.length === 1) {
        const lastAspectRatio = currentRow[0].width / currentRow[0].height;
        let lastBaseHeight = BASE_HEIGHT;
        
        if (lastAspectRatio < VERTICAL_RATIO) {
          lastBaseHeight = BASE_HEIGHT * 1.5;
        } else if (lastAspectRatio > HORIZONTAL_RATIO) {
          lastBaseHeight = BASE_HEIGHT * 0.7;
        }

        row.height = lastBaseHeight;
        row.items[0].height = lastBaseHeight;
        row.items[0].width = lastAspectRatio * lastBaseHeight;
      }

      rows.push(row);
      currentRow = [];
      currentRowWidth = 0;
    }
  });

  // 计算总高度
  const totalHeight = rows.reduce((height, row) => height + row.height + SPACING, 0) - SPACING;

  return {
    rows,
    totalHeight
  };
});

// 格式化日期显示
function formatDate(date: string | number): string {
  const timestamp = typeof date === 'string' ? parseInt(date) : date;
  const dateObj = new Date(timestamp * 1000);
  return `${dateObj.toLocaleDateString()} ${dateObj.toLocaleTimeString()}`;
}

// 格式化文件大小显示
function formatFileSize(bytes: number): string {
  const units = ['B', 'KB', 'MB', 'GB'];
  let size = bytes;
  let unitIndex = 0;
  
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024;
    unitIndex++;
  }
  
  return `${size.toFixed(1)} ${units[unitIndex]}`;
}

// 处理选择按钮点击
const handleSelectButtonClick = (screenshot: Screenshot, event: Event) => {
  event.stopPropagation() // 阻止事件冒泡
  if (!selectionStore.isSelectionMode) {
    selectionStore.enterSelectionMode()
  }
  selectionStore.toggleSelection(screenshot.id)
}
</script>

<template>
  <div 
    class="screenshot-gallery" 
    ref="containerRef"
    :style="{ height: layout.totalHeight + 'px' }"
  >
    <!-- 相册选择弹窗 -->
    <album-selection-dialog
      :show="showAlbumDialog"
      :selected-count="selectionStore.selectedCount"
      @update:show="showAlbumDialog = $event"
      @select="handleAddToAlbum"
    />

    <!-- 预览组件 -->
    <screenshot-preview
      v-model="showPreview"
      v-model:currentIndex="previewIndex"
      :screenshots="props.screenshots"
      :initial-index="previewIndex"
    />

    <!-- 多选模式下的底部工具栏 -->
    <div
      v-if="selectionStore.isSelectionMode"
      class="selection-toolbar"
    >
      <n-space justify="space-between" align="center">
        <span>已选择 {{ selectionStore.selectedCount }} 项</span>
        <n-space>
          <n-button @click="selectionStore.exitSelectionMode" secondary circle>
            <template #icon>
              <n-icon><Close /></n-icon>
            </template>
          </n-button>
          <n-button
            type="primary"
            :disabled="selectionStore.selectedCount === 0"
            @click="showAlbumDialog = true"
          >
            <template #icon>
              <n-icon><Albums /></n-icon>
            </template>
            添加到相册
          </n-button>
        </n-space>
      </n-space>
    </div>

    <!-- 行容器 -->
    <div
      v-for="(row, rowIndex) in layout.rows"
      :key="`row-${rowIndex}`"
      class="gallery-row"
      :style="{ 
        height: `${row.height}px`,
        marginBottom: rowIndex < layout.rows.length - 1 ? `${SPACING}px` : '0'
      }"
    >
      <!-- 图片项 -->
      <div
        v-for="(item, itemIndex) in row.items"
        :key="item.screenshot.id"
        class="gallery-item"
        :class="{ 'is-selected': selectionStore.isSelected(item.screenshot.id) }"
        :style="{
          width: `${item.width}px`,
          height: `${item.height}px`,
          marginLeft: itemIndex > 0 ? `${SPACING}px` : '0'
        }"
        @click="handleImageClick(props.screenshots.indexOf(item.screenshot))"
      >
        <!-- 选择按钮 -->
        <n-tooltip trigger="hover">
          <template #trigger>
            <div
              class="selection-button"
              :class="{ 
                'is-visible': selectionStore.isSelectionMode,
                'is-selected': selectionStore.isSelected(item.screenshot.id)
              }"
              @click="handleSelectButtonClick(item.screenshot, $event)"
            >
              <n-icon size="28">
                <CheckmarkCircle v-if="selectionStore.isSelected(item.screenshot.id)" />
                <RadioButtonOff v-else />
              </n-icon>
            </div>
          </template>
          {{ selectionStore.isSelectionMode ? '选择/取消选择' : '开始选择' }}
        </n-tooltip>

        <!-- 图片 -->
        <n-image
          :src="item.screenshot.thumbnailPath"
          :alt="item.screenshot.filename"
          class="gallery-image"
          object-fit="cover"
          preview-disabled
          lazy
        />
        
        <!-- 信息遮罩层 -->
        <div class="info-overlay">
          <div class="screenshot-info">
            <n-ellipsis class="filename">{{ item.screenshot.filename }}</n-ellipsis>
            <div class="screenshot-metadata">
              <span>{{ formatDate(item.screenshot.created_at) }}</span>
              <span>{{ formatFileSize(item.screenshot.file_size) }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 画廊容器 */
.screenshot-gallery {
  width: 100%;
  position: relative;
}

/* 行容器 */
.gallery-row {
  display: flex;
  width: 100%;
}

/* 图片项容器 */
.gallery-item {
  position: relative;
  overflow: hidden;
  background-color: rgba(0, 0, 0, 0.03);
  transition: transform 0.3s ease;
  cursor: pointer;
}

/* 图片样式 */
.gallery-image {
  width: 100%;
  height: 100%;
  transition: transform 0.3s ease;
}

/* 悬停效果 */
.gallery-item:hover .gallery-image {
  transform: scale(1.05);
}

/* 信息遮罩层 */
.info-overlay {
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  background: linear-gradient(transparent, rgba(0, 0, 0, 0.75));
  padding: 12px;
  color: white;
  opacity: 0;
  transition: opacity 0.2s ease;
}

.gallery-item:hover .info-overlay {
  opacity: 1;
}

/* 信息布局 */
.screenshot-info {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

/* 文件名样式 */
.filename {
  font-size: 0.9em;
  font-weight: 500;
  line-height: 1.4;
}

/* 元数据样式 */
.screenshot-metadata {
  display: flex;
  gap: 8px;
  font-size: 0.8em;
  opacity: 0.9;
}

.selection-toolbar {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  background: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(10px);
  padding: 12px 16px;
  box-shadow: 0 -2px 12px rgba(0, 0, 0, 0.08);
  z-index: 100;
  animation: slideUp 0.3s ease;
}

@keyframes slideUp {
  from {
    transform: translateY(100%);
  }
  to {
    transform: translateY(0);
  }
}

.selection-button {
  position: absolute;
  top: 8px;
  left: 8px;
  z-index: 2;
  opacity: 0;
  transition: all 0.2s ease;
  background: rgba(255, 255, 255, 0.9);
  border-radius: 50%;
  width: 28px;
  height: 28px;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  color: #8a8a8a;
}

.selection-button:hover {
  background: white;
  transform: scale(1.1);
  color: var(--primary-color);
}

.selection-button.is-selected {
  color: var(--primary-color);
  background: white;
}

.gallery-item:hover .selection-button {
  opacity: 1;
}

.selection-button.is-visible {
  opacity: 1;
}

.gallery-item.is-selected {
  outline: 2px solid var(--primary-color);
  outline-offset: -2px;
}

.gallery-item.is-selected .gallery-image {
  opacity: 0.9;
}
</style> 