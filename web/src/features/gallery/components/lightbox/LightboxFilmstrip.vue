<script setup lang="ts">
import { ref, computed, watch, nextTick } from 'vue'
import { useGalleryStore } from '../../store'
import { useGalleryLightbox } from '../../composables'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { galleryApi } from '../../api'

const store = useGalleryStore()
const lightbox = useGalleryLightbox()

const filmstripRef = ref<HTMLElement | null>(null)

// 缩略图配置
const THUMBNAIL_SIZE = 80
const THUMBNAIL_GAP = 8

const assets = computed(() => store.lightbox.assets)
const currentIndex = computed(() => store.lightbox.currentIndex)
const selectedIds = computed(() => store.selection.selectedIds)
const selectedCount = computed(() => selectedIds.value.size)

// 虚拟滚动配置
const virtualizer = useVirtualizer({
  count: assets.value.length,
  getScrollElement: () => filmstripRef.value,
  estimateSize: () => THUMBNAIL_SIZE + THUMBNAIL_GAP,
  horizontal: true,
  overscan: 5,
})

// 滚动到当前图片
function scrollToCurrent() {
  nextTick(() => {
    virtualizer.value.scrollToIndex(currentIndex.value, {
      align: 'center',
    })
  })
}

// 监听当前索引变化，自动滚动
watch(currentIndex, () => {
  scrollToCurrent()
})

// 点击缩略图
function handleThumbnailClick(index: number, event: MouseEvent) {
  if (event.ctrlKey || event.metaKey) {
    // Ctrl/Cmd + 点击：多选
    const asset = assets.value[index]
    if (asset) {
      // 使用全局选择
      const isSelected = selectedIds.value.has(asset.id)
      store.selectAsset(asset.id, !isSelected, true)
    }
  } else {
    // 普通点击：跳转
    lightbox.goToIndex(index)
  }
}

// 检查是否选中
function isSelected(index: number): boolean {
  const asset = assets.value[index]
  return asset ? selectedIds.value.has(asset.id) : false
}

// 获取缩略图URL
function getThumbnailUrl(asset: any) {
  return galleryApi.getAssetThumbnailUrl(asset)
}

// 隐藏filmstrip
function handleToggleFilmstrip() {
  lightbox.toggleFilmstrip()
}
</script>

<template>
  <div class="flex flex-col border-t border-border bg-card/80 backdrop-blur-md">
    <!-- 控制栏 -->
    <div class="flex items-center justify-between border-b border-border px-4 py-2">
      <button
        class="inline-flex items-center gap-1.5 rounded px-2 py-1 text-xs text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
        @click="handleToggleFilmstrip"
      >
        <svg class="h-4 w-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M19 9l-7 7-7-7"
          />
        </svg>
        <span>隐藏</span>
      </button>

      <div class="flex items-center gap-3 text-xs text-muted-foreground">
        <span v-if="selectedCount > 0" class="font-medium text-primary">
          已选 {{ selectedCount }} 项
        </span>
        <span>{{ currentIndex + 1 }} / {{ assets.length }}</span>
      </div>
    </div>

    <!-- 缩略图滚动容器 -->
    <div
      ref="filmstripRef"
      class="filmstrip-scroller min-w-0 overflow-x-auto overflow-y-hidden px-4 py-3"
    >
      <div
        :style="{
          width: `${virtualizer.getTotalSize()}px`,

          position: 'relative',
        }"
      >
        <div
          v-for="item in virtualizer.getVirtualItems()"
          :key="item.index"
          :data-index="item.index"
          :style="{
            position: 'absolute',
            top: 0,
            left: 0,
            width: `${THUMBNAIL_SIZE}px`,
            height: `${THUMBNAIL_SIZE}px`,
            transform: `translateX(${item.start}px)`,
          }"
        >
          <div
            class="filmstrip-thumbnail group relative cursor-pointer overflow-hidden rounded transition-all duration-200"
            :class="{
              'scale-110 ring-2 ring-primary': item.index === currentIndex,
              'ring-2 ring-primary': isSelected(item.index),
              'hover:ring-2 hover:ring-border':
                item.index !== currentIndex && !isSelected(item.index),
            }"
            @click="handleThumbnailClick(item.index, $event)"
          >
            <img
              :src="getThumbnailUrl(assets[item.index])"
              :alt="assets[item.index]?.name"
              class="h-full w-full object-cover"
            />

            <!-- 遮罩层 -->
            <div
              class="absolute inset-0 bg-transparent transition-all group-hover:bg-foreground/10"
              :class="{
                'bg-foreground/20': item.index === currentIndex || isSelected(item.index),
              }"
            />

            <!-- 选择指示器 -->
            <div
              v-if="isSelected(item.index)"
              class="absolute top-1 right-1 rounded-full bg-primary p-0.5 text-primary-foreground"
            >
              <svg class="h-3 w-3" fill="currentColor" viewBox="0 0 20 20">
                <path
                  fill-rule="evenodd"
                  d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z"
                  clip-rule="evenodd"
                />
              </svg>
            </div>

            <!-- 当前指示器 -->
            <div
              v-if="item.index === currentIndex"
              class="pointer-events-none absolute inset-0 border-2 border-primary"
            />
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.filmstrip-scroller {
  height: 100px;
  scrollbar-width: thin;
  scrollbar-color: hsl(var(--border)) transparent;
}

.filmstrip-scroller::-webkit-scrollbar {
  height: 6px;
}

.filmstrip-scroller::-webkit-scrollbar-thumb {
  background: hsl(var(--border));
  border-radius: 3px;
}

.filmstrip-scroller::-webkit-scrollbar-thumb:hover {
  background: hsl(var(--muted-foreground));
}

.filmstrip-thumbnail {
  transition:
    transform 0.2s ease,
    box-shadow 0.2s ease;
}
</style>
