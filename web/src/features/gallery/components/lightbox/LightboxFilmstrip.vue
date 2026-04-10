<script setup lang="ts">
import { ref, computed, watch, nextTick, onMounted } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { Play } from 'lucide-vue-next'
import { galleryApi } from '../../api'
import { useGalleryContextMenu, useGalleryData, useGallerySelection } from '../../composables'
import { useGalleryStore } from '../../store'
import ScrollArea from '@/components/ui/scroll-area/ScrollArea.vue'
import ScrollBar from '@/components/ui/scroll-area/ScrollBar.vue'
import MediaStatusChips from '../MediaStatusChips.vue'

const store = useGalleryStore()
const gallerySelection = useGallerySelection()
const galleryData = useGalleryData()
const galleryContextMenu = useGalleryContextMenu()

const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const filmstripRef = ref<HTMLElement | null>(null)
const loadingPages = ref<Set<number>>(new Set())

const THUMBNAIL_SIZE = 64
const THUMBNAIL_GAP = 12

const totalCount = computed(() => store.totalCount)
const currentIndex = computed(() => store.selection.activeIndex ?? 0)
const selectedIds = computed(() => store.selection.selectedIds)
const perPage = computed(() => store.perPage)

// 胶片条使用横向虚拟列表，只渲染可见缩略图，避免灯箱场景下全量节点开销。
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

watch(scrollAreaRef, (newRef) => {
  if (newRef) {
    // ScrollArea 的真实横向滚动容器在挂载后才可用。
    filmstripRef.value = newRef.viewportElement
  }
})

function getAssetAtIndex(index: number) {
  return store.getAssetsInRange(index, index)[0]
}

const virtualItems = computed(() => virtualizer.value.getVirtualItems())

const filmstripItems = computed(() => {
  // 把模板里重复读取的派生字段前置到 computed，降低渲染阶段重复计算。
  return virtualItems.value.map((item) => {
    const asset = getAssetAtIndex(item.index)
    const isSelected = asset ? selectedIds.value.has(asset.id) : false
    return {
      ...item,
      asset,
      thumbnailUrl: asset ? galleryApi.getAssetThumbnailUrl(asset) : '',
      isSelected,
      isVideo: asset?.type === 'video',
      isCurrent: item.index === currentIndex.value,
      rating: asset?.rating ?? 0,
      reviewFlag: asset?.reviewFlag ?? 'none',
    }
  })
})

async function loadMissingData(
  items: ReturnType<typeof virtualizer.value.getVirtualItems>
): Promise<void> {
  if (items.length === 0) {
    return
  }

  const visibleIndexes = items.map((item) => item.index)
  const neededPages = new Set(visibleIndexes.map((idx) => Math.floor(idx / perPage.value) + 1))
  const loadPromises: Promise<void>[] = []

  neededPages.forEach((pageNum) => {
    if (!store.isPageLoaded(pageNum) && !loadingPages.value.has(pageNum)) {
      // 可见区按页懒加载，避免滚动触发重复请求同一页。
      loadingPages.value.add(pageNum)
      loadPromises.push(
        galleryData.loadPage(pageNum).finally(() => {
          loadingPages.value.delete(pageNum)
        })
      )
    }
  })

  if (loadPromises.length > 0) {
    await Promise.all(loadPromises)
  }
}

watch(
  () => ({
    items: virtualizer.value.getVirtualItems(),
    totalCount: totalCount.value,
  }),
  async ({ items }) => {
    await loadMissingData(items)
  }
)

onMounted(() => {
  // 首次进入灯箱时让当前 active 资产尽量位于胶片条可视区域中心。
  nextTick(() => {
    virtualizer.value.scrollToIndex(currentIndex.value, {
      align: 'center',
    })
  })
})

watch(currentIndex, (newIndex) => {
  // 灯箱切图时保持胶片条跟随当前索引。
  nextTick(() => {
    virtualizer.value.scrollToIndex(newIndex, {
      align: 'auto',
    })
  })
})

function handleThumbnailClick(index: number, event: MouseEvent) {
  const asset = getAssetAtIndex(index)
  if (!asset) {
    return
  }

  void gallerySelection.handleAssetClick(asset, event, index)
}

function handleThumbnailContextMenu(index: number, event: MouseEvent) {
  const asset = getAssetAtIndex(index)
  if (!asset) {
    return
  }

  event.preventDefault()
  event.stopPropagation()
  // 先同步 selection 语义，再打开共享右键菜单，和主视图保持一致。
  void gallerySelection.handleAssetContextMenu(asset, event, index).then(() => {
    galleryContextMenu.openForAsset({ asset, event, index, sourceView: 'filmstrip' })
  })
}

function handleWheel(event: WheelEvent) {
  if (filmstripRef.value) {
    // 统一把纵向滚轮映射为横向滚动，提升鼠标滚轮浏览胶片条的效率。
    filmstripRef.value.scrollLeft += event.deltaY
  }
}
</script>

<template>
  <div class="surface-top flex flex-col border-t border-border">
    <ScrollArea ref="scrollAreaRef" class="filmstrip-scroller" @wheel.prevent="handleWheel">
      <div class="px-4 py-3">
        <div
          :style="{
            width: `${virtualizer.getTotalSize()}px`,
            position: 'relative',
          }"
        >
          <div
            v-for="item in filmstripItems"
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
              v-if="item.asset"
              class="filmstrip-thumbnail group relative h-full w-full cursor-pointer overflow-hidden rounded transition-all duration-200 select-none"
              :class="{
                'scale-110 ring-2 ring-primary ring-offset-4': item.isCurrent,
                'bg-foreground/20': item.isSelected,
              }"
              @click="handleThumbnailClick(item.index, $event)"
              @contextmenu="handleThumbnailContextMenu(item.index, $event)"
              @selectstart.prevent
            >
              <img
                :src="item.thumbnailUrl"
                :alt="item.asset.name"
                class="h-full w-full object-contain object-center"
                draggable="false"
                @dragstart.prevent
              />

              <div
                class="absolute inset-0 bg-transparent transition-all group-hover:bg-foreground/10"
                :class="{
                  'bg-foreground/12': item.isSelected,
                  'bg-foreground/18': item.isCurrent,
                }"
              />

              <div
                v-if="item.isVideo"
                class="absolute inset-x-0 bottom-0 flex items-end justify-start bg-gradient-to-t from-black/55 via-black/10 to-transparent p-1.5"
              >
                <div
                  class="flex h-5 w-5 items-center justify-center rounded-full border border-white/20 bg-black/60 text-white shadow-sm"
                >
                  <Play class="ml-px h-2.5 w-2.5 fill-current" />
                </div>
              </div>

              <MediaStatusChips :rating="item.rating" :review-flag="item.reviewFlag" compact />

              <div
                v-if="item.isSelected"
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

            <div v-else class="h-full w-full animate-pulse rounded bg-muted" />
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
  user-select: none;
  -webkit-user-select: none;
  transition:
    transform 0.2s ease,
    box-shadow 0.2s ease;
}
</style>
