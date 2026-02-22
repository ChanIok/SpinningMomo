<script setup lang="ts">
import { ref, computed, watch, nextTick, onMounted } from 'vue'
import { useGalleryStore } from '../../store'
import { useGalleryLightbox } from '../../composables'
import { useGalleryData } from '../../composables'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { galleryApi } from '../../api'
import type { Asset } from '../../types'
import ScrollArea from '@/components/ui/scroll-area/ScrollArea.vue'
import ScrollBar from '@/components/ui/scroll-area/ScrollBar.vue'

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const galleryData = useGalleryData()

const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const filmstripRef = ref<HTMLElement | null>(null)

// 缩略图配置
const THUMBNAIL_SIZE = 64
const THUMBNAIL_GAP = 12

// 总数从 store.totalCount 获取
const totalCount = computed(() => store.totalCount)
const currentIndex = computed(() => store.lightbox.currentIndex)
const selectedIds = computed(() => store.selection.selectedIds)
const perPage = computed(() => store.perPage)

// 虚拟滚动配置
const virtualizer = useVirtualizer({
  get count() {
    return totalCount.value
  },
  getScrollElement: () => filmstripRef.value,
  estimateSize: () => THUMBNAIL_SIZE + THUMBNAIL_GAP,
  horizontal: true,
  scrollPaddingEnd: 24,
  scrollPaddingStart: 6,
  overscan: 5,
})

// 初始化滚动容器引用
watch(scrollAreaRef, (newRef) => {
  if (newRef) {
    filmstripRef.value = newRef.viewportElement
  }
})

// 虚拟资产列表
const virtualAssets = ref<(Asset | null | undefined)[]>([])

// 加载状态跟踪（防止重复加载）
const loadingPages = ref<Set<number>>(new Set())

/**
 * 根据当前可见的虚拟项和总数，更新 virtualAssets
 */
function syncVirtualAssets(items: ReturnType<typeof virtualizer.value.getVirtualItems>) {
  if (items.length === 0) {
    virtualAssets.value = []
    return
  }

  virtualAssets.value = items.map((item) => {
    const index = item.index
    const asset = store.getAssetsInRange(index, index)[0]
    return asset
  })
}

/**
 * 异步加载缺失的数据
 */
async function loadMissingData(
  items: ReturnType<typeof virtualizer.value.getVirtualItems>
): Promise<void> {
  if (items.length === 0) return

  // 收集所有可见的索引
  const visibleIndexes = items.map((item) => item.index)

  // 计算需要的页
  const neededPages = new Set(visibleIndexes.map((idx) => Math.floor(idx / perPage.value) + 1))

  // 加载缺失的页
  const loadPromises: Promise<void>[] = []
  neededPages.forEach((pageNum) => {
    if (!store.isPageLoaded(pageNum) && !loadingPages.value.has(pageNum)) {
      loadingPages.value.add(pageNum)
      const loadPromise = galleryData.loadPage(pageNum).finally(() => {
        loadingPages.value.delete(pageNum)
      })
      loadPromises.push(loadPromise)
    }
  })

  // 等待所有加载完成
  if (loadPromises.length > 0) {
    await Promise.all(loadPromises)
  }
}

/**
 * 统一监听器：监听 UI 变化（滚动、总数变化）
 */
watch(
  () => ({
    items: virtualizer.value.getVirtualItems(),
    totalCount: totalCount.value,
  }),
  async ({ items }) => {
    // 1️⃣ 立即同步更新 UI（即使数据未加载，先显示骨架屏）
    syncVirtualAssets(items)

    // 2️⃣ 异步加载缺失的数据，加载完成后再次同步 UI
    await loadMissingData(items)

    // 3️⃣ 数据加载完成后，手动触发一次 UI 同步（更新已加载的数据）
    syncVirtualAssets(virtualizer.value.getVirtualItems())
  }
)

// 初始化时滚动到当前索引
onMounted(() => {
  nextTick(() => {
    virtualizer.value.scrollToIndex(currentIndex.value, {
      align: 'center',
    })
  })
})

// 监听 currentIndex 变化，自动滚动到可见位置
watch(currentIndex, (newIndex) => {
  nextTick(() => {
    virtualizer.value.scrollToIndex(newIndex, {
      align: 'auto', // 智能滚动：只有不可见时才滚动
    })
  })
})

// 点击缩略图
function handleThumbnailClick(index: number, event: MouseEvent) {
  const asset = store.getAssetsInRange(index, index)[0]
  if (!asset) return // 数据未加载，直接返回

  if (event.ctrlKey || event.metaKey) {
    // Ctrl/Cmd + 点击：多选
    const isSelected = selectedIds.value.has(asset.id)
    store.selectAsset(asset.id, !isSelected, true)

    // 多选后，根据选中数量更新详情面板
    if (store.selectedCount > 1) {
      store.setDetailsFocus({ type: 'batch' })
    } else if (store.selectedCount === 1) {
      store.setDetailsFocus({ type: 'asset', asset })
    } else {
      store.clearDetailsFocus()
    }
  } else {
    // 普通点击：只跳转，详情更新由 LightboxImage 的 watch 自动处理
    lightbox.goToIndex(index)
  }
}

// 检查是否选中
function isSelected(index: number): boolean {
  const asset = store.getAssetsInRange(index, index)[0]
  return asset ? selectedIds.value.has(asset.id) : false
}

// 获取缩略图URL
function getThumbnailUrl(asset: any) {
  return galleryApi.getAssetThumbnailUrl(asset)
}

// 处理鼠标滚轮事件，转换为横向滚动
function handleWheel(event: WheelEvent) {
  if (filmstripRef.value) {
    filmstripRef.value.scrollLeft += event.deltaY
  }
}
</script>

<template>
  <div class="surface-top flex flex-col border-t border-border">
    <!-- 缩略图滚动容器 -->
    <ScrollArea ref="scrollAreaRef" class="filmstrip-scroller" @wheel.prevent="handleWheel">
      <div class="px-4 py-3">
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
              class="filmstrip-thumbnail group relative h-full w-full cursor-pointer overflow-hidden rounded transition-all duration-200"
              :class="{
                'scale-110 bg-foreground/20 ring-1 ring-primary ring-offset-4':
                  item.index === currentIndex,
                'bg-foreground/20': isSelected(item.index),
              }"
              @click="handleThumbnailClick(item.index, $event)"
            >
              <img
                v-if="store.getAssetsInRange(item.index, item.index)[0]"
                :src="getThumbnailUrl(store.getAssetsInRange(item.index, item.index)[0]!)"
                :alt="store.getAssetsInRange(item.index, item.index)[0]?.name"
                class="h-full w-full object-contain object-center"
              />
              <!-- 骨架屏 -->
              <div v-else class="h-full w-full animate-pulse bg-muted" />

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
            </div>
          </div>
        </div>
      </div>
      <ScrollBar orientation="horizontal" />
    </ScrollArea>
  </div>
</template>

<style scoped>
.filmstrip-scroller {
  height: 100px;
}

.filmstrip-thumbnail {
  transition:
    transform 0.2s ease,
    box-shadow 0.2s ease;
}
</style>
